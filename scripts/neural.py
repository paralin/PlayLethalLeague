import LethalLeague as ll

class LethalLeagueOutput:
    def write(self, txt):
        ll.log(txt)

    def flush(self):
        pass
    
import sys
import os.path
import time
import pickle
import numpy as np
import random

sys.stdout = sys.stderr = LethalLeagueOutput()
def log(txt):
    print("[Python] " + txt)

from keras.models import Sequential
from keras.layers.core import Dense
from keras.layers.advanced_activations import ELU
from keras.optimizers import Adam
from itertools import permutations
from copy import copy

def to_range(val, maxval):
    return max(min(2.0 * ((val - 0.5 * maxval) / maxval), 1.0), -1.0)

def softmax(values, temperature):
    e = np.exp(values / temperature)
    return e / np.sum(e)

class Experience:
    def __init__(self, state, action, reward, new_state, terminal):
        self.state = state
        self.action = action
        self.reward = reward
        self.new_state = new_state
        self.terminal=terminal

class PlayerState:
    def __init__(self, idx, game, inter, observe_only):
        self.idx = idx
        self.game = game
        self.inter = inter
        self.player = inter.player

        self.hitsBeforeSwing = 0
        self.wasSwinging = False
        self.last_animation_state_countdown = 0
        self.hit_count = 0

        self.previous_action = None
        self.previous_states = []
        
        self.is_ai = False
        if not observe_only:
            self.observe_actions = self.is_ai = idx != 0
        else:
            self.observe_actions = True
            
        self.game.setInputOverride(idx, not self.observe_actions)
        
        self.round_winners = []
        self.round_winners_size = 100
        self._reset_previous_states()
        
    def _get_player_state(self, idx):
        game_data = self.game.gameData
        player = player_0 = game_data.player(idx)
        
        playerChargeTimer = -1.0
        if player.state.character_state == 1:
            playerChargeTimer = to_range(player.state.change_animation_state_countdown, 15000)
        
        # Player coords (2)
        # Player velocity (2)
        # Player charge timer (1)
        return [
            # Coords (2)
            to_range(player_0.coords.xcoord - self.inter.stageX, self.inter.stageXSize), to_range(player_0.coords.ycoord - self.inter.stageY, self.inter.stageYSize),
            # Velocity X/Y
            min(max(player_0.state.horizontal_speed / (player_0.base.max_speed / 60.0), -1.0), 1.0), min(max(player_0.state.vertical_speed / (player_0.base.fast_fall_speed / 60.0), -1.0), 1.0),
            playerChargeTimer
        ]

    def _get_state(self):
        game_data = self.game.gameData

        ballSpeed = game_data.ball_state.ballSpeed
        ballState = game_data.ball_state.state

        correctedBallSpeed = to_range(ballSpeed if ballState is 0 else 0, 1200.0)
        ballTag = ord(game_data.ball_state.ballTag)
        if ballTag != 0:
            ballTag = 1

        # Invert the tag if we're on the other side, this way both are the same in the learning
        if self.idx != 0:
            ballTag = 1 - ballTag

        # special_meter is multiplied by 1638400. divide by 1638400 to get out of 8.
        # check charge animation (1) and changeAnimationStateCountdown (max for charge 15000) which is im assuming 1.5 seconds
        # for switch, character state 4 + animation state 43 = switchflipping

        ball_direction = np.array([game_data.ball_state.xspeed, game_data.ball_state.yspeed], dtype=np.float)
        if np.linalg.norm(ball_direction) != 0:
            ball_direction /= np.linalg.norm(ball_direction)

        state = []
        # Player 0 state (5)
        state += self._get_player_state(self.idx)
        # Player 1 state (5)
        state += self._get_player_state(1 - self.idx)
        # Ball coordinates (2)
        # Ball speed (1)
        # Ball direction (2)
        # Ball tag (1)
        state += [
            to_range(game_data.ball_coord.xcoord - self.inter.stageX, self.inter.stageXSize), to_range(game_data.ball_coord.ycoord - self.inter.stageY, self.inter.stageYSize),
            correctedBallSpeed,
            ball_direction[0],
            ball_direction[1],
            ballTag
        ]
        return np.array(state)

    def log(self, txt):
        log("[" + str(self.idx) + "] " + txt)

    def _get_reward(self):
        '''
        Gets the reward between now and the last call to this function
        '''
        reward = 0
        game_data = self.game.gameData
        player_0 = game_data.player(self.idx)

        # Give a negative reward for missing a swing
        # This is buggy for non-player
        # just disable it for now
        if not self.is_ai and False:
            isSwinging = player_0.state.character_state == 1
            if self.wasSwinging and not isSwinging:
                if player_0.state.total_hit_counter == self.hitsBeforeSwing:
                    self.log("Missed a swing, rewarding -1.")
                    reward -= 1
        self.recordSwingState()

        # Give a positive reward for hitting the ball
        hit_count = player_0.state.total_hit_counter
        if hit_count != self.hit_count:
            # lastCountdown = 1 is better
            lastCountdown = 1.0 - min(max(self.last_animation_state_countdown / 15000.0, 0.0), 1.0)
            chargeBonus = lastCountdown * 10
            if lastCountdown < 0.1:
                chargeBonus = 0
            self.log("Hit the ball, rewarding 10 + charge bonus of " + str(chargeBonus))
            reward += (hit_count - self.hit_count) * 10
            reward += chargeBonus
            self.hit_count = hit_count

        if abs(reward) > 0.5:
            self.log("Reward " + str(reward) + " at time " + str(ll.get_tick_count()))

        return reward

    def recordSwingState(self):
        game_data = self.game.gameData
        player_0 = game_data.player(self.idx)
        # We are swinging if character state = 0x01 = 1
        self.wasSwinging = player_0.state.character_state is 1
        if self.wasSwinging:
            self.hitsBeforeSwing = player_0.state.total_hit_counter

    def endOfLife(self):
        self.wasSwinging = False
        self.hitsBeforeSwing = self.game.gameData.player(self.idx).state.total_hit_counter

    def _reset_previous_states(self):
        '''
        Zeros all previous states
        '''
        self.previous_states = [np.zeros(self.inter.state_size) for i in range(self.inter.state_count)]

    def _add_previous_state(self, state):
        '''
        Adds a new state to the previous states and removes the oldest
        '''
        self.previous_states.pop(0)
        self.previous_states.append(state)

    def _get_previous_states(self):
        '''
        Returns all previous states as one vector
        '''
        return np.array(self.previous_states).flatten()

    def _add_round_winner(self, won):
        if len(self.round_winners) >= self.round_winners_size:
            self.round_winners.pop(0)
        self.round_winners.append(1 if won else 0)

    def _get_win_percentage(self):
        if len(self.round_winners) == 0:
            return 0.0

        return sum(self.round_winners) / len(self.round_winners)

    def _do_playing(self, forceReward=None, terminal=False):
        state = self._get_state()
        previous_previous_states = self._get_previous_states()
        self._add_previous_state(state)
        previous_states = self._get_previous_states()

        # Get the reward and add s, a, r, s' as an experience
        if self.previous_action != None:
            reward = forceReward if forceReward != None else self._get_reward()

            # Add an experience to the player
            # Heavily favor experiences where the ball is tagged to the enemy
            #stateBefore = self.previous_states[len(self.previous_states) - 2]
            #enemyTagged = stateBefore[9] == 1
            #if abs(reward) > 0.5 or random.uniform(0, 1) > (0.9 if not enemyTagged else 0.2):
            self.player.add_experience(previous_previous_states, self.previous_action, reward, previous_states, terminal)

            # Reset all previous states
            if terminal:
                self._reset_previous_states()

        # Choose an action to perform this step
        chosen_action = None
        if self.observe_actions:
            # just observe what we're currently pressing
            # player_inputs can be called ONCE PER PLAYONCE CALL PER PLAYER THIS IS IMPORTANT
            inps = self.game.gameData.player_inputs(self.idx)
            curr_action = [bool(elem) for elem in inps]
            # It's possible they're pressing some "exclusive" buttons simultanously, check for this
            # up/down exclusive
            if curr_action[0]:
                curr_action[1] = False
            # left/right exclusive
            if curr_action[2]:
                curr_action[3] = False
            # ok find the matching action in the list
            actidx = self.inter.actions_idx.get(str(curr_action), None)
            if actidx is None:
                self.log("Unable to find matching action for " + str(curr_action))
            #else:
                #if curr_action[4]:
                #    self.log("Observed attack action.")
                #if actidx != 53:
                #    self.log("Observed action " + str(actidx) + " = " + str(curr_action))
            chosen_action = actidx
        else:
            #predicted_q = self.player.predict_q(previous_states)
            predicted_q = self.player.predict_q(state)

            # Either sample a random action or choose the best one
            if self.inter.random_enabled:
                chosen_action = np.random.choice(self.inter.action_size, p=softmax(predicted_q, self.random_action_temperature))
            else:
                chosen_action = np.argmax(predicted_q)

            self._apply_action(chosen_action)

        # Save the state and the action taken
        self.previous_state = state
        self.previous_action = chosen_action

        player_0 = self.game.gameData.player(self.idx)
        if player_0.state.character_state is 1:
            self.last_animation_state_countdown = player_0.state.change_animation_state_countdown

    def _apply_action(self, action):
        '''
        Presses the hotkeys corresponding to the passed action for player
        '''
        action_hotkeys = self.inter.actions[action]

        for hotkey_id, hotkey in enumerate(LethalInterface.hotkeys):
            self.game.setInputImmediate(hotkey, action_hotkeys[hotkey_id])

class ReinforcementLearner:
    def __init__(self, batch_size, state_size, action_size, learn_rate, discount_factor, dimensionality):
        self.state_size = state_size
        self.action_size = action_size
        self.batch_size = batch_size

        self.experiences = []

        self.discount_factor = discount_factor
        
        # Build the model
        self.model = Sequential()

        self.model.add(Dense(input_dim=state_size, output_dim=dimensionality))
        self.model.add(ELU())
        self.model.add(Dense(output_dim=dimensionality))
        self.model.add(ELU())
        self.model.add(Dense(output_dim=dimensionality))
        self.model.add(ELU())
        self.model.add(Dense(output_dim=dimensionality))
        self.model.add(ELU())
        self.model.add(Dense(output_dim=action_size))

        self.model.compile(loss="mse", optimizer=Adam(lr=learn_rate))

        # The creator is responsible for calling load weights and load experiences

    def predict_q(self, state):
        '''
        Predict the Q values for a single state
        '''

        state_batch = [state]
        return self.model.predict(state_batch)[0]

    def add_experience(self, state, action, reward, new_state, terminal):
        self.experiences.append(Experience(state, action, reward, new_state, terminal))

    def load(self, path):
        if os.path.exists(path):
            log("Loading weights from " + path)
            self.model.load_weights(path)

    def saveExperiences(self, path):
        log("Dumping experiences to " + path)
        with open(path, "wb") as f:
            pickle.dump(self.experiences, f)

class LethalInterface:
    hotkeys = [
        ll.CONTROL_UP, ll.CONTROL_DOWN,
        ll.CONTROL_LEFT, ll.CONTROL_RIGHT,
        ll.CONTROL_ATTACK, ll.CONTROL_BUNT,
        ll.CONTROL_JUMP,
    ]

    def __init__(self, game, scripts_root):
        self.game = game
        self.previous_action = None
        self.new_match_called_at_least_once = False
        self.scripts_root = scripts_root
        
        self.experiences_path = scripts_root + "experiences/"
        if not os.path.exists(self.experiences_path):
            os.makedirs(self.experiences_path)
    
        self.player_states = []

        # Build all possible combinations
        self.actions = []
        self.actions_idx = {}
        normal_action = [[True, True], [True, False], [False, True], [False, False]]
        exclusive_action = [[True, False], [False, True], [False, False]]
        ixx = 0
        for horizontal in exclusive_action:
            for vertical in exclusive_action:
                for execution in normal_action:
                    for jump in [[True,], [False,]]:
                        action = horizontal + vertical + execution + jump
                        # make and hash a str for fast lookup later
                        self.actions_idx[str(action)] = ixx
                        ixx += 1
                        self.actions.append(action)

        self.random_enabled = False
        self.random_action_temperature = 0.9 # Low: All actions equally likely, High: Higher Q more likely

        self.state_count = 2
        self.state_size = 20
        self.action_size = len(self.actions)
        self.learn_rate = 0.002
        self.discount_factor = 0.9
        self.update_interval = 20
        self.dimensionality = 300
        self.disable_experience_dump = False
        
        self.currently_in_game = False

        # We want 1 batch size for the player
        self.player = ReinforcementLearner(1, self.state_size, self.action_size, self.learn_rate, self.discount_factor, self.dimensionality)
        log("Initialized ReinforcementLearner player with state size " + str(1) + " and action size " + str(self.action_size))

        # Load the experiences and weights
        self.weights_path = scripts_root + "weights.dat"
        self.player.load(self.weights_path)
        if os.path.exists(self.weights_path):
            self.lastweightmt = time.ctime(os.path.getmtime(self.weights_path))
        else:
            self.lastweightmt = None

    def newMatchStarted(self):
        log("newMatchStarted called")
        self.currently_in_game = True
        self.was_playing = False
        
        is_online = False
        
        if ord(self.game.gameData.is_online) == 1 and False:
            is_online = True
            log("Detected online match")
        self.is_online = is_online

        self.player_states = [PlayerState(i, self.game, self, is_online) for i in range(2)]

        # calculate the map size
        coord_offset = 50000.0
        game_data = self.game.gameData
        stage = game_data.stage_base

        self.stageX = stage.x_origin * coord_offset
        self.stageXSize = stage.x_size * coord_offset
        self.stageY = stage.y_origin * coord_offset
        self.stageYSize = stage.y_size * coord_offset
        self.stageDiagonal = np.sqrt(self.stageXSize**2 + self.stageYSize**2)

        self.base_lives = game_data.player(0).state.lives
        self.was_playing = False

        self.last_update_time = 0
        self.next_update_time = 0
        self.new_match_called_at_least_once = True

    def matchReset(self):
        self.currently_in_game = False

    def playOneFrame(self):
        nowTicks = ll.get_tick_count()
        if not self.new_match_called_at_least_once:
            return nowTicks + 10

        self.last_update_time = nowTicks
        self.next_update_time = nowTicks + self.update_interval

        # First check if we are actually in play
        game_data = self.game.gameData
        player_0 = game_data.player(0)
        player_1 = game_data.player(1)

        playingNow = game_data.ball_state.state != 14 and game_data.ball_state.state != 1

        if not playingNow and self.was_playing:
            log("== end of life ==")
            self.player_states[0].endOfLife()
            self.player_states[1].endOfLife()

            weDied = player_0.state.lives < player_1.state.lives
            winnerIdx = 1 if weDied else 0
            self.player_states[winnerIdx]._do_playing(30, True)
            self.player_states[winnerIdx]._add_round_winner(True)
            self.player_states[1 - winnerIdx]._do_playing(-30, True)
            self.player_states[1 - winnerIdx]._add_round_winner(False)

            if not self.is_online:
                self.game.respawnPlayer(1 - winnerIdx)
                self.game.resetBall()

            print("Win percentage:", self.player_states[0]._get_win_percentage())
            
            # dump experiences
            if len(self.player.experiences) > 50 and not self.disable_experience_dump:
                log("Dumped experiences.")
                with open(self.experiences_path + str(ll.get_tick_count()) + '.exp', 'wb') as handle:
                    pickle.dump(self.player.experiences, handle)
                    self.player.experiences = []
            
            # Check if the weights file has change
            if os.path.exists(self.weights_path):
                weightmt = time.ctime(os.path.getmtime(self.weights_path))
                if self.lastweightmt != weightmt:
                    self.player.load(self.weights_path)
                    self.lastweightmt = weightmt
                    log("Reloaded weights from file.")

        self.was_playing = playingNow
        # for now just bail out if we're not actually in play
        if not playingNow:
            if not self.is_online:
                self.game.setPlayerLives(0, self.base_lives)
                self.game.setPlayerLives(1, self.base_lives)
            return self.next_update_time

        # Calculate things like total swing count, etc
        self.player_states[0]._do_playing()
        self.player_states[1]._do_playing()

        return self.next_update_time

