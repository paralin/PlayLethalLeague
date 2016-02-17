try:
    import LethalLeague as ll
except:
    print("Could not import LethalLeague, importing dummy")
    from dummy import DummyLethalLeague as ll
    
class LethalLeagueOutput:
    def write(self, txt):
        ll.log(txt)

import sys
import os.path
sys.stdout = sys.stderr = LethalLeagueOutput()

def log(txt):
    print("[Python] " + txt)
        
from keras.models import Sequential
from keras.layers.core import Dense
from keras.layers.recurrent import GRU
from keras.optimizers import RMSprop

import numpy as np
from itertools import permutations
from copy import copy
import random

def to_range(val, maxval):
    return max(min(2.0 * ((val - 0.5 * maxval) / maxval), 1.0), -1.0)

class ReinforcementLearner:
    def __init__(self, state_size, action_size, learn_rate=0.03):
        self.state_size = state_size
        self.action_size = action_size

        # Build the model
        self.model = Sequential()
        '''
        self.model.add(Dense(input_dim=state_size, output_dim=50))
        self.model.add(Dense(output_dim=50, activation="tanh"))
        self.model.add(Dense(output_dim=action_size))
        '''

        self.model.add(GRU(action_size, return_sequences=False, batch_input_shape=(1, 1, state_size), stateful=True))
        self.model.add(Dense(action_size))
        
        self.model.compile(loss="mse", optimizer=RMSprop(lr=learn_rate))
        self.load("weights.dat")

    def predict_q(self, state):
        return self.model.predict(state.reshape(1, 1, self.state_size)).reshape(self.action_size)
    
    def learn(self, state, action_qs):
        self.model.fit(state.reshape(1, 1, self.state_size), action_qs.reshape(1, self.action_size), nb_epoch=1, verbose=0)

    def load(self, path):
        if os.path.exists(path):
            log("Loading from " + path)
            self.model.load_weights(path)
    
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
        
        # Build all possible combinations
        # 2^7 - 3 * 2^5 = 32 (Up/Down, Left/Right, Atk/Jump are exclusive)
        self.actions = []
        exclusive_action = [[True, False], [False, True], [False, False]]
        for horizontal in exclusive_action:
            for vertical in exclusive_action:
                for execution in exclusive_action:
                    for jump in [[True,], [False,]]:
                        action = horizontal + vertical + execution + jump
                        self.actions.append(action)

        self.state_size = 12
        self.action_size = len(self.actions)
        self.learner = ReinforcementLearner(self.state_size, self.action_size)
        log("Initialized ReinforcementLearner with state size " + str(self.state_size) + " and action size " + str(self.action_size))

    def recordSwingState(self):
        game_data = self.game.gameData
        player_0 = game_data.player(0)
        # We are swinging if character state = 0x01 = 1
        self.wasSwinging = player_0.state.character_state is 1
        if self.wasSwinging:
            self.hitsBeforeSwing = player_0.state.total_hit_counter
            
    def newMatchStarted(self):
        log("newMatchStarted called")
        self.new_match_called_at_least_once = True
        
        self.hit_count = 0
        self.previous_action = None
        self.was_playing = False
        
        # calculate the map size
        coord_offset = 50000.0
        game_data = self.game.gameData
        stage = game_data.stage_base
        self.stageX = stage.x_origin * coord_offset
        self.stageXSize = stage.x_size * coord_offset
        self.stageY = stage.y_origin * coord_offset
        self.stageYSize = stage.y_size * coord_offset
        self.stageDiagonal = np.sqrt(self.stageXSize**2 + self.stageYSize**2)

        self.distance_to_ball = None
        self.base_lives = game_data.player(0).state.lives
        self.was_playing = False
        
        self.hitsBeforeSwing = game_data.player(0).state.total_hit_counter
        self.wasSwinging = False
        self.last_animation_state_countdown = 0
        self.last_update_time = 0

        self.discount_factor = 0.98

    def playOneFrame(self):
        if not self.new_match_called_at_least_once:
            return
        nowTicks = ll.get_tick_count()
        
        # Limit update to once every 10ms
        if self.last_update_time + 50 > nowTicks:
            return
        self.last_update_time = nowTicks

        # First check if we are actually in play
        game_data = self.game.gameData
        player_0 = game_data.player(0)
        player_1 = game_data.player(1)

        playingNow = player_0.state.respawn_timer is 0 and game_data.ball_state.state != 14 and game_data.ball_state.state != 1
                
        # Decide to spawn the second player or not
        trainHitOnly = False
        #self.game.setPlayerExists(1, not trainHitOnly)
        
        # Add logic here for resetting player lives / checking life count
        # Before setting trainHitOnly to false.
        # see https://github.com/paralin/PlayLethalLeague/blob/master/PlayLethalLeague/LLNeural.cpp#L158'
        if not playingNow and self.was_playing:
            log("== end of life ==")
            self.wasSwinging = False
            self.hitsBeforeSwing = player_0.state.total_hit_counter
            weDied = player_0.state.lives < player_1.state.lives
            if weDied:
                log("We died, rewarding -30.")
                self._do_learning(-30, True)
                self.game.respawnPlayer(0)
            else:
                log("We got a kill, rewarding 20.")
                self._do_learning(20, True)
                self.game.respawnPlayer(1)
            
            self.game.resetBall()
            self.learner.save("weights.dat")
        
        self.was_playing = playingNow
        # for now just bail out if we're not actually in play
        if not playingNow:
            self.game.setPlayerLives(0, self.base_lives)
            self.game.setPlayerLives(1, self.base_lives)
            return
            
        # Calculate things like total swing count, etc
        self._do_learning()
        if player_0.state.character_state is 1:
            self.last_animation_state_countdown = player_0.state.change_animation_state_countdown

    
    def _do_learning(self, forceReward=None, terminal=False):
        state = self._get_state()

        # Predict the Q values for all actions in the current state
        # and get the action with the highest Q value
        predicted_q = self.learner.predict_q(state)
        best_action = np.argmax(predicted_q)

        # Random action chance
        if random.randrange(0, 100) < 10:
            log("Doing something random.")
            best_action = random.randrange(0, self.action_size)
            
        best_action_q = predicted_q[best_action]
        # Train the model on the temporal difference error
        # (PredictedQ + Reward) - NewMaxQ
        if self.previous_action != None:
            reward = forceReward if forceReward != None else self._get_reward()
            # Actual Q = Current max Q + reward
            new_q = self.discount_factor * best_action_q + reward if not terminal else reward

            self.previous_qs[self.previous_action] = new_q

            self.learner.learn(self.previous_state, self.previous_qs)
            self.previous_action = None

        self._apply_action(best_action)
        
        # Save the state, predicted Q-values and chosen action for learning
        self.previous_state = state
        self.previous_qs = predicted_q
        self.previous_action = best_action

    def _apply_action(self, action):
        '''
        Presses the hotkeys corresponding to the passed action
        '''
        #if action < 0 or action >= self.action_size:
        #     print("Invalid action id", action)

        # A boolean for every hotkey index (pressed/released)
        action_hotkeys = self.actions[action]
        #print("Applying action", " ".join(["D" if a else "U" for a in action_hotkeys]))

        for hotkey_id, hotkey in enumerate(LethalInterface.hotkeys):
            self.game.setInputImmediate(hotkey, action_hotkeys[hotkey_id])
        
    def _get_state(self):
        game_data = self.game.gameData
        player_0 = game_data.player(0)
        player_1 = game_data.player(1)
        
        ballSpeed = game_data.ball_state.ballSpeed
        ballState = game_data.ball_state.state
        
        correctedBallSpeed = to_range(ballSpeed if ballState is 0 else 0, 1200.0)
        
        playerChargeTimer = -1.0
        if player_0.state.character_state == 1:
            playerChargeTimer = to_range(player_0.state.change_animation_state_countdown, 15000)
        
        # Player 0 coords (2)
        # Player 0 velocity (2)
        # Player 0 charge timer (1) 
        # Player 1 coords (2)
        # Ball coordinates (2)
        # Ball speed (1)
        # Ball direction (2)
        
        # special_meter is multiplied by 1638400. divide by 1638400 to get out of 8.
        # check charge animation (1) and changeAnimationStateCountdown (max for charge 15000) which is im assuming 1.5 seconds
        # for switch, character state 4 + animation state 43 = switchflipping

        ball_direction = np.array([game_data.ball_state.xspeed, game_data.ball_state.yspeed], dtype=np.float)
        if np.linalg.norm(ball_direction) != 0:
            ball_direction /= np.linalg.norm(ball_direction)

        return np.array([
        # Player 0 coords
            to_range(player_0.coords.xcoord - self.stageX, self.stageXSize), to_range(player_0.coords.ycoord - self.stageY, self.stageYSize),
        # Player 0 velocity, could be -max_speed, +max_speed, just divide by max_speed and min/max it
            min(max(player_0.state.horizontal_speed / (player_0.base.max_speed / 60.0), -1.0), 1.0), min(max(player_0.state.vertical_speed / (player_0.base.fast_fall_speed / 60.0), -1.0), 1.0),
        # Player 0 charge timer
            playerChargeTimer,
            to_range(player_1.coords.xcoord - self.stageX, self.stageXSize), to_range(player_1.coords.ycoord - self.stageY, self.stageYSize),
            to_range(game_data.ball_coord.xcoord - self.stageX, self.stageXSize), to_range(game_data.ball_coord.ycoord - self.stageY, self.stageYSize),
            correctedBallSpeed,
            ball_direction[0],
            ball_direction[1]
        ])

    def _get_reward(self):
        '''
        Gets the reward between now and the last call to this function
        '''
        reward = 0
        game_data = self.game.gameData
        player_0 = game_data.player(0)

        # Give a negative reward for missing a swing
        isSwinging = player_0.state.character_state == 1
        if self.wasSwinging and not isSwinging:
            if player_0.state.total_hit_counter == self.hitsBeforeSwing:
                log("Missed a swing, rewarding -1.")
                reward -= 1
        self.recordSwingState()
        
        # Give a positive reward for hitting the ball
        hit_count = self.game.gameData.player(0).state.total_hit_counter
        if hit_count != self.hit_count:
            # lastCountdown = 1 is better
            lastCountdown = 1.0 - min(max(self.last_animation_state_countdown / 15000.0, 0.0), 1.0)
            chargeBonus = lastCountdown * 10
            if lastCountdown < 0.1:
                chargeBonus = 0
            log("We hit the ball, rewarding 10 + charge bonus of " + str(chargeBonus))
            reward += (hit_count - self.hit_count) * 10
            reward += chargeBonus
            self.hit_count = hit_count
            
        # Give a reward for moving towards the ball
        player_x = np.float(player_0.coords.xcoord)
        player_y = np.float(player_0.coords.ycoord)
        ball_x = np.float(game_data.ball_coord.xcoord)
        ball_y = np.float(game_data.ball_coord.ycoord)

        '''
        distance_to_ball = np.sqrt((player_x - ball_x)**2 + (player_y - ball_y)**2) / self.stageDiagonal
        if self.distance_to_ball != None:
            reward += self.distance_to_ball - distance_to_ball
        self.distance_to_ball = distance_to_ball
        '''

        if reward != 0:
            log("Reward " + str(reward) + " at time " + str(ll.get_tick_count()))

        return reward