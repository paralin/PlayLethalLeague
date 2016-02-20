try:
    import LethalLeague as ll
except:
    print("Could not import LethalLeague, importing dummy")
    from dummy import DummyLethalLeague as ll

class LethalLeagueOutput:
    def write(self, txt):
        ll.log(txt)

    def flush(self):
        pass

import sys
import os.path
sys.stdout = sys.stderr = LethalLeagueOutput()

def log(txt):
    print("[Python] " + txt)

from keras.models import Sequential
from keras.layers.core import Dense
from keras.layers.advanced_activations import ELU
from keras.optimizers import Adam

import pickle
import numpy as np
from itertools import permutations
from copy import copy
import random

def to_range(val, maxval):
    return max(min(2.0 * ((val - 0.5 * maxval) / maxval), 1.0), -1.0)

class Experience:
    def __init__(self, state, action, reward, new_state, terminal):
        self.state = state
        self.action = action
        self.reward = reward
        self.new_state = new_state
        self.terminal=terminal

class PlayerState:
    def __init__(self, idx, game, inter):
        self.idx = idx
        self.game = game
        self.inter = inter
        self.learner = inter.learner

        self.hitsBeforeSwing = 0
        self.wasSwinging = False
        self.last_animation_state_countdown = 0
        self.hit_count = 0

        self.previous_action = None
        self.previous_states = []

        self.observe_actions = idx != 0

        self.round_winners = []
        self.round_winners_size = 100
        self._reset_previous_states()

    def _get_state(self):
        game_data = self.game.gameData
        player_0 = game_data.player(self.idx)
        player_1 = game_data.player(1 - self.idx)

        ballSpeed = game_data.ball_state.ballSpeed
        ballState = game_data.ball_state.state

        correctedBallSpeed = to_range(ballSpeed if ballState is 0 else 0, 1200.0)

        playerChargeTimer = -1.0
        if player_0.state.character_state == 1:
            playerChargeTimer = to_range(player_0.state.change_animation_state_countdown, 15000)

        # Player 0 coords (2)
        # Player 0 velocity (2)
        # Player 0 charge timer (1)
        # player 0 facing direction (1)
        # Player 1 coords (2)
        # Player 1 velocity (2)
        # Ball coordinates (2)
        # Ball speed (1)
        # Ball direction (2)
        # Ball tag (1)

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

        return np.array([
        # Player 0 coords
            to_range(player_0.coords.xcoord - self.inter.stageX, self.inter.stageXSize), to_range(player_0.coords.ycoord - self.inter.stageY, self.inter.stageYSize),
        # Player 0 velocity, could be -max_speed, +max_speed, just divide by max_speed and min/max it
            min(max(player_0.state.horizontal_speed / (player_0.base.max_speed / 60.0), -1.0), 1.0), min(max(player_0.state.vertical_speed / (player_0.base.fast_fall_speed / 60.0), -1.0), 1.0),
        # Player 0 charge timer
            playerChargeTimer,
            player_0.state.facing_direction,
            to_range(player_1.coords.xcoord - self.inter.stageX, self.inter.stageXSize), to_range(player_1.coords.ycoord - self.inter.stageY, self.inter.stageYSize),
        # Player 1 velocity, could be -max_speed, +max_speed, just divide by max_speed and min/max it
            min(max(player_1.state.horizontal_speed / (player_1.base.max_speed / 60.0), -1.0), 1.0), min(max(player_1.state.vertical_speed / (player_1.base.fast_fall_speed / 60.0), -1.0), 1.0),
            to_range(game_data.ball_coord.xcoord - self.inter.stageX, self.inter.stageXSize), to_range(game_data.ball_coord.ycoord - self.inter.stageY, self.inter.stageYSize),
            correctedBallSpeed,
            ball_direction[0],
            ball_direction[1],
            ballTag
        ])

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
        if not self.observe_actions:
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
            stateBefore = self.previous_states[len(self.previous_states) - 2]
            enemyTagged = stateBefore[9] == 1
            if abs(reward) > 0.5 or random.uniform(0, 1) > (0.9 if not enemyTagged else 0.2):
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
            # Predict the Q values for all actions in the current state
            # and get the action with the highest Q value.
            # Small random action chance.
            if not self.inter.random_enabled or random.uniform(0, 1) >= self.inter.random_action_chance:
                predicted_q = self.player.predict_q(previous_states)
                chosen_action = np.argmax(predicted_q)
            else:
                chosen_action = random.randrange(0, self.inter.action_size)

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
        #if action < 0 or action >= self.action_size:
        #     print("Invalid action id", action)

        # A boolean for every hotkey index (pressed/released)
        action_hotkeys = self.inter.actions[action]

        for hotkey_id, hotkey in enumerate(LethalInterface.hotkeys):
            self.game.setInputImmediate(hotkey, action_hotkeys[hotkey_id])

class ReinforcementLearner:
    def __init__(self, batch_size, state_size, action_size, learn_rate, discount_factor, dimensionality):
        self.state_size = state_size
        self.action_size = action_size
        self.batch_size = batch_size

        self.experiences = []
        self.max_experiences = 100000

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

        # Add zeros to fill for batch size
        state_batch = np.zeros((self.batch_size, self.state_size))
        state_batch[0] = state

        return self.model.predict(state_batch)[0]

    def add_experience(self, state, action, reward, new_state, terminal):
        now = ll.get_tick_count()

        # Keep only a "few" experiences, remove the oldest one
        if len(self.experiences) >= self.max_experiences:
            self.experiences.pop(0)

        self.experiences.append(Experience(state, action, reward, new_state, terminal))

    def experience_replay(self):
        '''
        Perform a single experience replay learning step
        '''

        # Wait until we have enough experiences
        if len(self.experiences) < self.batch_size:
            return False

        # Get random experiences
        experiences = []
        for i in range(self.batch_size - 1):
            experiences.append(self.experiences[random.randrange(0, len(self.experiences) - 1)])
        # Also append the latest experience
        experiences.append(self.experiences[len(self.experiences) - 1])

        # Put all experiences data into numpy arrays
        states = np.array([ex.state for ex in experiences]).reshape(self.batch_size, self.state_size)
        rewards = np.array([ex.reward for ex in experiences])
        new_states = np.array([ex.new_state for ex in experiences]).reshape(self.batch_size, self.state_size)

        # First predict the Q values for the old state, then predict
        # the Qs of the new state and set the actual Q of the chosen
        # action for each batch
        actual_qs = self.model.predict(states)

        predicted_new_qs = self.model.predict(new_states)

        for i in range(self.batch_size):
            chosen_action = experiences[i].action
            actual_qs[i][chosen_action] = self.discount_factor * predicted_new_qs[i][chosen_action] + rewards[i] if not experiences[i].terminal else experiences[i].reward

        self.model.fit(states, actual_qs, nb_epoch=1, verbose=0)
        return True

    def load(self, path):
        if os.path.exists(path):
            log("Loading weights from " + path)
            self.model.load_weights(path)

    def loadExperiences(self, path):
        if os.path.exists(path):
            log("Loading experiences from " + path)
            with open(path, "rb") as f:
                self.experiences = pickle.load(f)

    def saveExperiences(self, path):
        log("Dumping experiences to " + path)
        with open(path, "wb") as f:
            pickle.dump(self.experiences, f)

    def save(self, path):
        self.model.save_weights(path, True)

class LethalInterface:
    hotkeys = [
        ll.CONTROL_UP, ll.CONTROL_DOWN,
        ll.CONTROL_LEFT, ll.CONTROL_RIGHT,
        ll.CONTROL_ATTACK, ll.CONTROL_BUNT,
        ll.CONTROL_JUMP,
    ]

    def __init__(self, game):
        self.game = game
        self.previous_action = None
        self.new_match_called_at_least_once = False
        self.player_states = []

        # Build all possible combinations
        # 2^7 - 3 * 2^5 = 32 (Up/Down, Left/Right, Atk/Jump are exclusive)
        self.actions = []
        self.actions_idx = {}
        exclusive_action = [[True, False], [False, True], [False, False]]
        ixx = 0
        for horizontal in exclusive_action:
            for vertical in exclusive_action:
                for execution in exclusive_action:
                    for jump in [[True,], [False,]]:
                        action = horizontal + vertical + execution + jump
                        # make and hash a str for fast lookup later
                        self.actions_idx[str(action)] = ixx
                        ixx += 1
                        self.actions.append(action)

        self.learning = True
        self.random_action_chance = 0.1

        # Batch size used for learner
        self.batch_size = 32
        # We want 5 seconds worth of learning each time
        self.target_batch_time = 5000

        self.state_count = 4
        self.state_size = 15
        self.action_size = len(self.actions)
        self.learn_rate = 0.001
        self.discount_factor = 0.9
        self.update_interval = 30
        self.dimensionality = 200
        self.random_enabled = False
        self.currently_in_game = False
        # Assume that the time to run 1 batcn is 90% of the update interval to be safe initially
        self.average_batch_time = 0.9 * self.update_interval
        self.batch_times = []

        # We want 1 batch size for the player
        self.player = ReinforcementLearner(1, self.state_count * self.state_size, self.action_size, self.learn_rate, self.discount_factor, self.dimensionality)
        log("Initialized ReinforcementLearner player with state size " + str(1) + " and action size " + str(self.action_size))

        self.learner = ReinforcementLearner(self.batch_size, self.state_count * self.state_size, self.action_size, self.learn_rate, self.discount_factor, self.dimensionality)
        log("Initialized ReinforcementLearner learner with state size " + str(self.state_size) + " and action size " + str(self.action_size))

        # Load the experiences and weights
        self.learner.loadExperiences("experiences.dat")
        self.learner.load("weights.dat")

        # Copy to player
        self.applyLearning(False)

    def newMatchStarted(self):
        log("newMatchStarted called")
        self.new_match_called_at_least_once = True
        self.currently_in_game = True
        self.was_playing = False

        self.player_states = [PlayerState(i, self.game, self) for i in range(2)]

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

    def matchReset(self):
        self.currently_in_game = False

    # Returning True will result in a applyLearning
    def learnOneFrame(self):
        if not self.learning:
            return False
        nowTicks = ll.get_tick_count()

        # Perform a batch
        if not self.learner.experience_replay():
            return False

        afterTicks = ll.get_tick_count()
        # Happens sometimes randomly
        if afterTicks - nowTicks < 5:
            return False

        self.batch_times.append(afterTicks - nowTicks)
        self.average_batch_time = np.mean(self.batch_times)

        if len(self.batch_times) > 5:
            if self.average_batch_time > self.target_batch_time - 100 and self.learner.batch_size > 10:
                self.learner.batch_size -= 1
                log("Lowering batch size to " + str(self.learner.batch_size) + ", last timing was " + str(self.average_batch_time))
            elif self.average_batch_time < self.target_batch_time + 100 and len(self.learner.experiences) > self.learner.batch_size:
                self.learner.batch_size += 1
                log("Raising batch size to " + str(self.learner.batch_size) + ", last timing was " + str(self.average_batch_time))
            self.batch_times.pop(0)

        # Learning happened, we want to apply it
        return True

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

            self.game.respawnPlayer(1 - winnerIdx)
            self.game.resetBall()

            print("Win percentage:", self.player_states[0]._get_win_percentage())

        self.was_playing = playingNow
        # for now just bail out if we're not actually in play
        if not playingNow:
            self.game.setPlayerLives(0, self.base_lives)
            self.game.setPlayerLives(1, self.base_lives)
            return self.next_update_time

        # Calculate things like total swing count, etc
        self.player_states[0]._do_playing()
        self.player_states[1]._do_playing()

        return self.next_update_time

    def applyLearning(self, do_weights_save=True):
        log("Applying learning from learner to player.")

        # Save weights
        if do_weights_save:
            self.learner.save("weights.dat")
        self.player.load("weights.dat")

        # Transfer experiences
        self.learner.experiences.extend(self.player.experiences)
        self.player.experiences = []
