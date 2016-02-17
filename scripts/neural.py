try:
	import LethalLeague as ll
except:
	print("Could not import LethalLeague, importing dummy")
	from dummy import DummyLethalLeague as ll
	
class LethalLeagueOutput:
	def write(self, txt):
		ll.log(txt)

import sys
sys.stdout = sys.stderr = LethalLeagueOutput()
		
from keras.models import Sequential
from keras.layers.core import Dense, Activation
from keras.optimizers import RMSprop
import numpy as np
from itertools import permutations
from copy import copy

def _to_input_range(val, maxval):
	return max(min(2.0 * ((val - 0.5 * maxval) / maxval), 1.0), -1.0)

class ReinforcementLearner:
	def __init__(self, state_size, action_size, learn_rate=0.01):
		# Build the model
		self.model = Sequential()
		self.model.add(Dense(input_dim=state_size, output_dim=50, activation="relu"))
		self.model.add(Dense(output_dim=50, activation="relu"))
		self.model.add(Dense(output_dim=action_size))
		self.model.compile(loss="mse", optimizer=RMSprop(lr=learn_rate))

	def predict_q(self, state):
		return self.model.predict(state)
	
	def learn(self, state, action_qs):
		self.model.fit(state, action_qs, nb_epoch=1, verbose=0)

	def save(self, path):
		self.model.save_weights(path)

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

		self.state_size = 6
		self.action_size = len(self.actions)
		self.learner = ReinforcementLearner(self.state_size, self.action_size)
		ll.log("Initialized ReinforcementLearner with state size "
			   + str(self.state_size) + " and action size " + str(self.action_size))

	def newMatchStarted(self):
		ll.log("newMatchStarted called")
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

	def playOneFrame(self):
		if not self.new_match_called_at_least_once:
			return
			
		# First check if we are actually in play
		game_data = self.game.gameData
		playingNow = game_data.player(0).state.respawn_timer is 0 and game_data.ball_state.state != 14 and game_data.ball_state.state != 1
		
		# Decide to spawn the second player or not
		trainHitOnly = True
		self.game.setPlayerExists(1, not trainHitOnly)
		
		# Add logic here for resetting player lives / checking life count
		# Before setting trainHitOnly to false.
		# see https://github.com/paralin/PlayLethalLeague/blob/master/PlayLethalLeague/LLNeural.cpp#L158'
		
		# for now just bail out if we're not actually in play
		if not playingNow:
			return
		
		state = self._get_state()

		# Predict the Q values for all actions in the current state
		# and get the action with the highest Q value
		predicted_q = self.learner.predict_q(state)
		best_action = np.argmax(predicted_q)
		best_action_q = predicted_q[0][best_action]

		# Train the model on the temporal difference error
		# (PredictedQ + Reward) - NewMaxQ
		if self.previous_action != None:
			reward = self._get_reward()

			# Actual Q = Current max Q + reward
			self.previous_qs[self.previous_action] = best_action_q + reward

			self.learner.learn(self.previous_state, self.previous_qs)
		
		self._apply_action(best_action)
		
		# Save the state, predicted Q-values and chosen action for learning
		self.previous_state = state
		self.previous_qs = predicted_q
		self.previous_action = best_action

	def _apply_action(self, action):
		'''
		Presses the hotkeys corresponding to the passed action
		'''
		if action < 0 or action >= self.action_size:
			ll.log("Invalid action id " + str(action))

		# A boolean for every hotkey index (pressed/released)
		action_hotkeys = self.actions[action]

		for hotkey_id, hotkey in enumerate(LethalInterface.hotkeys):
			ll.setInputImmediate(hotkey, action_hotkeys[hotkey_id])
		
	def _get_state(self):
		game_data = self.game.gameData
		player_0 = game_data.player(0)
		player_1 = game_data.player(1)

		return np.array([
			to_input_range(game_data.ball_coord.xcoord - self.stageX, self.stageXSize), to_input_range(game_data.ball_coord.ycoord - self.stageY, self.stageYSize),
			to_input_range(player_0.coords.xcoord - self.stageX, self.stageXSize), to_input_range(player_0.coords.ycoord - self.stageY, self.stageYSize),
			# insert player1 here
		]).reshape((1, self.state_size))

	def _get_reward(self):
		'''
		Gets the reward between now and the last call to this function
		'''
		reward = 0

		# Give a positive reward for hitting the ball
		hit_count = self.game.gameData.player(0).state.total_hit_counter
		if hit_count != self.hit_count:
			reward += hit_count - self.hit_count

		if reward != 0:
			ll.log("Reward " + str(reward) + " at time " + ll.get_tick_count())

		return reward
