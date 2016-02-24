import sys
import os.path
ll = {'CONTROL_ATTACK': 16, 'CONTROL_UP': 1, 'CONTROL_RIGHT': 2, 'CONTROL_DOWN': 4, 'CONTROL_LEFT': 8, 'CONTROL_BUNT': 32, 'CONTROL_JUMP': 64, 'CONTROL_TAUNT': 128}

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
import time
import fnmatch

def to_range(val, maxval):
    return max(min(2.0 * ((val - 0.5 * maxval) / maxval), 1.0), -1.0)

class Experience:
    def __init__(self, state, action, reward, new_state, terminal):
        self.state = state
        self.action = action
        self.reward = reward
        self.new_state = new_state
        self.terminal=terminal

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
        
        self.last_files = []
        
    def load_experiences(self, fi, max_num):
        loaded = []
        with open(fi, "rb") as f:
            loaded = pickle.load(f)
        #log("Loading from " + fi)
        for i in range(min(len(loaded), max_num)):
            self.experiences.append(loaded[i])

    def experience_replay(self):
        '''
        Perform a single experience replay learning step
        '''
        
        self.experiences = []
        matches = []
        for root, dirnames, filenames in os.walk('experiences'):
            for filename in fnmatch.filter(filenames, '*.exp'):
                matches.append(os.path.join(root, filename))
        
        # see if there's any new files
        new_matches = [x for x in matches if x not in self.last_files]
        matches = self.last_files
        random.shuffle(matches)
        
        # load the new files
        for i in range(len(new_matches)):
            exp_so_far = len(self.experiences)
            if exp_so_far >= self.batch_size:
                break
            self.load_experiences(new_matches[i], self.batch_size - exp_so_far)
            
        if len(self.experiences) < self.batch_size:
            for i in range(len(matches)):
                exp_so_far = len(self.experiences)
                if exp_so_far >= self.batch_size:
                    break
                self.load_experiences(matches[i], self.batch_size - exp_so_far)
        
        if len(self.experiences) < self.batch_size:
            log("We have " + str(len(self.experiences)) + "/" + str(self.batch_size) + ", not enough to start.")
            time.sleep(2)
            return False
            
        experiences = self.experiences
        
        actual_batch_size = len(experiences)
        # Put all experiences data into numpy arrays
        states = np.array([ex.state for ex in experiences]).reshape(actual_batch_size, self.state_size)
        rewards = np.array([ex.reward for ex in experiences])
        new_states = np.array([ex.new_state for ex in experiences]).reshape(actual_batch_size, self.state_size)

        # First predict the Q values for the old state, then predict
        # the Qs of the new state and set the actual Q of the chosen
        # action for each batch
        actual_qs = self.model.predict(states)
        predicted_new_qs = self.model.predict(new_states)

        for i in range(len(experiences)):
            chosen_action = experiences[i].action
            actual_qs[i][chosen_action] = predicted_new_qs[i][chosen_action] + rewards[i] # self.discount_factor * predicted_new_qs[i][chosen_action] + rewards[i] if not experiences[i].terminal else experiences[i].reward

        self.model.fit(states, actual_qs, nb_epoch=1, verbose=0)
        return True

    def save(self, path):
        self.model.save_weights(path, True)

class LethalInterface:
    hotkeys = [
        ll["CONTROL_UP"], ll["CONTROL_DOWN"],
        ll["CONTROL_LEFT"], ll["CONTROL_RIGHT"],
        ll["CONTROL_ATTACK"], ll["CONTROL_BUNT"],
        ll["CONTROL_JUMP"]
    ]

    def __init__(self):
        self.batch_times = []
        
        # Build all possible combinations
        # 2^7 - 3 * 2^5 = 32 (Up/Down, Left/Right, Atk/Jump are exclusive)
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

        self.random_action_chance = 0.1

        # Batch size used for learner
        batch_size = 128
        # We want 2 seconds worth of learning each time
        self.target_batch_time = 3000

        self.state_count = 2
        self.state_size = 20
        self.action_size = len(self.actions)
        self.learn_rate = 0.002
        self.discount_factor = 0.9
        self.dimensionality = 300

        self.learner = ReinforcementLearner(batch_size, self.state_size, self.action_size, self.learn_rate, self.discount_factor, self.dimensionality)
        log("Initialized ReinforcementLearner learner with state size " + str(self.state_size) + " and action size " + str(self.action_size))

    # Returning True will result in a file save
    def learnOneFrame(self):
        nowTicks = time.clock() * 1000.0

        # Perform a batch
        if not self.learner.experience_replay():
            return False

        afterTicks = time.clock() * 1000.0

        self.batch_times.append(afterTicks - nowTicks)
        self.average_batch_time = np.mean(self.batch_times)


        if len(self.batch_times) > 5:
            self.batch_times.pop(0)
            '''
            if abs(self.target_batch_time - (afterTicks - nowTicks)) > 200:
                batches_per_ms = len(self.learner.experiences) / (afterTicks - nowTicks)
                log("Current batch per millsecond: " + str(batches_per_ms))
                self.learner.batch_size = int(batches_per_ms * self.target_batch_time)
                log("Re-calculated batch size of " + str(self.learner.batch_size))
            elif self.average_batch_time > self.target_batch_time - 100 and self.learner.batch_size > 10:
                self.learner.batch_size -= 1
                log("Lowering batch size to " + str(self.learner.batch_size) + ", last timing was " + str(self.average_batch_time))
            elif self.average_batch_time < self.target_batch_time + 100:
                self.learner.batch_size += 1
                log("Raising batch size to " + str(self.learner.batch_size) + ", last timing was " + str(self.average_batch_time))
        '''

        # Learning happened, we want to apply it
        log("Finished learning, took " + str(afterTicks - nowTicks) + " average timing " + str(self.average_batch_time) + " with " + str(len(self.batch_times)) + " times.")
        return True

    def saveWeights(self):
        self.learner.save("weights.dat")

if __name__ == "__main__":
    inter = LethalInterface()
    log("Starting learning.")
    while True:
        try:
            if inter.learnOneFrame():
                inter.saveWeights()
        except KeyboardInterrupt:
            sys.exit()