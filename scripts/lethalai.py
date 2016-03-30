import os
import sys
import pickle
import fnmatch
import random
import numpy as np
import math

from keras.models import Sequential
from keras.layers.core import Dense, Activation, Dropout
from keras.layers.advanced_activations import ELU, PReLU
from keras.optimizers import Adam, RMSprop
from keras.layers.recurrent import LSTM, SimpleRNN

def to_range(val, maxval):
    # Shouldnt this be
    # (Val - Max) / (Max - Min) = (Val - Max) / Max = Val/Max - 1 ?
    return max(min(2.0 * ((val - 0.5 * maxval) / maxval), 1.0), -1.0)

def softmax(values, temperature):
    e = np.exp(values / temperature)
    return e / np.sum(e)



# Build all possible combinations
_id_to_action = [] # ActionId -> ActionList
_action_to_id = {} # ActionStr -> ActionId
_move_only_actions = [] # Action ids

def id_to_action(id):
    return _id_to_action[id]

def action_to_id(action):
    return _action_to_id.get(str(action), None)

def get_action_count():
    return len(_id_to_action)

def _make_actions():
    normal_action = [[True, True], [True, False], [False, True], [False, False]]
    exclusive_action = [[True, False], [False, True], [False, False]]
    for horizontal in exclusive_action:
        for vertical in exclusive_action:
            for execution in normal_action:
                for jump in [[True,], [False,]]:
                    action = horizontal + vertical + execution + jump
                    idx = _action_to_id[str(action)] = len(_action_to_id)
                    _id_to_action.append(action)

                    # Determine if this is a movement only action
                    if not execution[0] and not execution[1] and not jump[0]:
                        _move_only_actions.append(idx)

_make_actions()

class Experience:
    def __init__(self, state, action, reward, new_state, terminal):
        self.state = state
        self.action = action
        self.reward = reward
        self.new_state = new_state
        self.terminal = terminal

# Experience sets are all experiences between significant reward events
class ExperienceSet:
    def __init__(self, target_memory, update_interval, experiences=None):
        self.target_memory = target_memory
        self.update_interval = update_interval

        self.reward_propogated = False
        self.experiences = experiences if experiences != None else []

        # Calculate how many to remember to hit target_memory
        # We should pass this as a param but whatever
        time_for_experience = update_interval # global_options["update_interval"]
        self.num_to_remember = int(target_memory / time_for_experience)
    
    # Add experience, make sure we only remember around 1 second
    def add_experience(self, experience):
        self.experiences.append(experience)
        if len(self.experiences) > self.num_to_remember:
            self.experiences.pop(0)

    def propogate_reward(self):
        num_experiences = len(self.experiences)
        if num_experiences >= 2 and not self.reward_propogated:
            reward = int(self.experiences[num_experiences - 1].reward)
            self.reward_propogated = True
            # Interpolate linearly from the start of the experience set to the end
            # ... except for the last one, obviously, since it already has the reward
            print("Propgated (" + str(reward) + "):", end="")
            for i in range(num_experiences - 1):
                experience = self.experiences[i]
                experience.reward += (i / (num_experiences - 1)) * reward
                experience.reward = int(experience.reward)
                print(" " + str(experience.reward), end="")
            print()
        self.reset()

    def reset(self):
        self.reward_propogated = False
        self.experiences = []

class ExperienceRecorder:
    def __init__(self, expected_state_size):
        self.expected_state_size = expected_state_size
        self.reset()

    def add_experience(self, state, action, reward, new_state, terminal):
        # Check for the correct state sizes
        if state.shape[0] != self.expected_state_size or new_state.shape[0] != self.expected_state_size:
            raise ValueError("Tried to add invalid state to exp recorder, expected size:", self.expected_state_size,
                  "actual sizes:", state.shape[0], new_state.shape[0])

        exp = Experience(state, action, reward, new_state, terminal)
        self.experiences.append(exp)
        return exp

    def get_random_experiences(self, count):
        # Return None if there are not enough experiences
        if(len(self.experiences) < count):
            return None

        return random.sample(self.experiences, count)

    def reset(self):
        self.experiences = []
        self.added_matches = []

    # todo: move this into ExperienceSet
    def experience_count(self):
        return len(self.experiences)

    def save_experiences(self, path):
        print("Dumping experiences to", path)
        with open(path, "wb") as f:
            pickle.dump(self.experiences, f)

    def load_experiences(self, path):
        loaded = []

        try:
            with open(path, "rb") as f:
                loaded = pickle.load(f)
        except:
            print("Error when loading experience at", path, ", skipping", sys.exc_info()[0])
            return
        
        if any([e.state.shape[0] != self.expected_state_size or e.new_state.shape[0] != self.expected_state_size for e in loaded]):
            print("Invalid state in experience at", path, ", skipping the complete file")
            return

        self.experiences += loaded

    def check_new_experiences(self):
        matches = []
        for root, dirnames, filenames in os.walk('experiences'):
            for filename in fnmatch.filter(filenames, '*.exp'):
                matches.append(os.path.join(root, filename))
        
        # see if there's any new files
        new_matches = [x for x in matches if x not in self.added_matches]
        self.added_matches += new_matches
        
        # load the new files
        for match in new_matches:
            self.load_experiences(match)
            n_ex = len(self.experiences)
            if n_ex > 500000:
                for i in range(n_ex - 500000):
                    self.experiences.pop(0)

class StateBundle:
    def __init__(self, state_size, state_count):
        self.state_size = state_size
        self.state_count = state_count
        self.states = []
        self.reset()

    def reset(self):
        '''
        Zeros all states
        '''
        self.states = [np.zeros(self.state_size) for i in range(self.state_count)]

    def add(self, state):
        '''
        Adds a new state to the states and removes the oldest
        '''
        self.states.pop(0)
        self.states.append(state)

    def get(self):
        '''
        Returns all states as one vector
        '''
        return np.array(self.states).flatten()

class BasePlayer:
    """
    Plays or observes the game and adds experiences to a learner
    """

    def __init__(self, player_id, game, exp_recorder, opts):
        self.player_id = player_id
        self.game = game
        self.exp_recorder = exp_recorder
        self.exp_set = ExperienceSet(opts["target_memory"], opts["update_interval"])

        # Copy some constants
        self.state_size = opts["state_size"]
        self.state_count = opts["state_count"]
        self.action_size = opts["action_size"]

        # Calculate the map size
        coord_offset = 50000.0
        stage = game.gameData.stage_base
        self.stage_loc = (stage.x_origin * coord_offset, stage.y_origin * coord_offset)
        self.stage_size = (stage.x_size * coord_offset, stage.y_size * coord_offset)

        # Create a state bundle which holds a few past states
        self.state_bundle = StateBundle(self.state_size, self.state_count)

        # Hit count variables
        self.hits_before_swing = 0
        self.was_swinging = False
        self.last_animation_state_countdown = 0
        self.hit_count = 0

        self.previous_action = None
        
        # Keeps track of the winners of the last N rounds
        self.round_winners = []
        self.round_winners_size = 100
        
    def _get_player_state(self, id, rel_to=None):
        '''
        Gets the state for a single player
        '''

        game_data = self.game.gameData
        player = player_0 = game_data.player(id)
        
        playerChargeTimer = -1.0
        if player.state.character_state == 1:
            playerChargeTimer = to_range(player.state.change_animation_state_countdown, 15000)
        
        # Player coords (2)
        # Player velocity (2)
        # Player charge timer (1)
        res = [
            # Coords (2)
            to_range(player_0.coords.xcoord - self.stage_loc[0], self.stage_size[0]), to_range(player_0.coords.ycoord - self.stage_loc[1], self.stage_size[1]),
            # Velocity X/Y
            min(max(player_0.state.horizontal_speed / (player_0.base.max_speed / 60.0), -1.0), 1.0), min(max(player_0.state.vertical_speed / (player_0.base.fast_fall_speed / 60.0), -1.0), 1.0),
            playerChargeTimer
        ]

        if rel_to != None:
            other = self._get_player_state(rel_to)
            res[0] -= other[0]
            res[1] -= other[1]

        return res

    def _get_state(self):
        '''
        Gets the complete state for one timestep
        '''

        game_data = self.game.gameData

        ballSpeed = game_data.ball_state.ballSpeed
        ballState = game_data.ball_state.state

        corrected_ball_speed = to_range(ballSpeed if ballState is 0 else 0, 1200.0)
        ball_tag = ord(game_data.ball_state.ballTag)
        if ball_tag != 0:
            ball_tag = 1

        # Invert the tag if we're on the other side, this way both are the same in the learning
        if self.player_id != 0:
            ball_tag = 1 - ball_tag

        # special_meter is multiplied by 1638400. divide by 1638400 to get out of 8.
        # check charge animation (1) and changeAnimationStateCountdown (max for charge 15000) which is im assuming 1.5 seconds
        # for switch, character state 4 + animation state 43 = switchflipping

        ball_direction = np.array([game_data.ball_state.xspeed, game_data.ball_state.yspeed], dtype=np.float)
        if np.linalg.norm(ball_direction) != 0:
            ball_direction /= np.linalg.norm(ball_direction)

        state = []
        me = game_data.player(self.player_id)
        # Player 0 state (5)
        state += self._get_player_state(self.player_id)
        # Player 1 state (5)
        state += self._get_player_state(1 - self.player_id)#, self.player_id)
        # Ball coordinates (2)
        # Ball speed (1)
        # Ball direction (2)
        # Ball tag (1)
        state += [
            to_range(game_data.ball_coord.xcoord - self.stage_loc[0], self.stage_size[0]), to_range(game_data.ball_coord.ycoord - self.stage_loc[1], self.stage_size[1]),
            corrected_ball_speed,
            ball_direction[0],
            ball_direction[1],
            ball_tag
        ]
        return np.array(state)

    def log(self, *txt):
        print("[Python Player " + str(self.player_id) + "]", *txt)

    def _get_reward(self, previous_action):
        '''
        Gets the reward between now and the last call to this function
        '''
        reward = 0
        game_data = self.game.gameData
        player_0 = game_data.player(self.player_id)

        '''
        # Give a negative reward for missing a swing
        # This is buggy for non-player
        # just disable it for now
        if not self.is_ai and False:
            is_swinging = player_0.state.character_state == 1
            if self.was_swinging and not is_swinging:
                if player_0.state.total_hit_counter == self.hits_before_swing:
                    self.log("Missed a swing, rewarding -1.")
                    reward -= 1
        '''

        self.record_swing_state()

        # Give a positive reward for hitting the ball
        hit_count = player_0.state.total_hit_counter
        if hit_count != self.hit_count:
            # lastCountdown = 1 is better
            last_countdown = 1.0 - min(max(self.last_animation_state_countdown / 15000.0, 0.0), 1.0)
            charge_bonus = last_countdown * 10
            if last_countdown < 0.1:
                charge_bonus = 0
            self.log("Hit the ball, rewarding 10 + charge bonus of ", charge_bonus)
            reward += (hit_count - self.hit_count) * 10
            reward += charge_bonus
            self.hit_count = hit_count

        if ord(player_0.state.touching_left_wall) > 0 or ord(player_0.state.touching_right_wall):
            reward -= 0.1 
        elif previous_action in _move_only_actions:
            reward += 0.1

        if abs(reward) > 0.5:
            self.log("Reward " + str(reward))

        return reward

    def record_swing_state(self):
        game_data = self.game.gameData
        player_0 = game_data.player(self.player_id)
        # We are swinging if character state = 0x01 = 1
        self.was_swinging = player_0.state.character_state is 1
        if self.was_swinging:
            self.hits_before_swing = player_0.state.total_hit_counter

    def end_of_life(self):
        self.was_swinging = False
        self.hits_before_swing = self.game.gameData.player(self.player_id).state.total_hit_counter

    def add_round_winner(self, won):
        if len(self.round_winners) >= self.round_winners_size:
            self.round_winners.pop(0)
        self.round_winners.append(1 if won else 0)

    def get_win_percentage(self):
        if len(self.round_winners) == 0:
            return 0.0

        return sum(self.round_winners) / len(self.round_winners)
    
    def _choose_action(self, bundled_states):
        print("Warn: base _chose_action called.")
        pass

    def update(self, forceReward=None, terminal=False):
        '''
        Updates the state, calls _choose_action and sets the current
        state and action as the previous
        '''

        # Get the old and new state bundles
        state = self._get_state()
        previous_bundled_states = self.state_bundle.get()
        self.state_bundle.add(state)
        bundled_states = self.state_bundle.get()

        # Get the reward and add s, a, r, s' as an experience
        if self.previous_action != None:
            reward = forceReward if forceReward != None else self._get_reward(self.previous_action)
            # Make sure it's an int
            reward = int(reward)

            # Add an experience to the experience recorder and set
            self.exp_set.add_experience(self.exp_recorder.add_experience(previous_bundled_states, self.previous_action, reward, bundled_states, terminal))

            # Determine if this is a significant reward (greater than 8 magnitude)
            if terminal or abs(reward) > 8.0:
                self.exp_set.propogate_reward()

            # Reset all previous states
            if terminal:
                print("Terminal reward with previous action (" + str(reward) + ")")
                self.state_bundle.reset()
                self.exp_set.reset()

        if self.previous_action is None and terminal:
            print("Terminal state, but previous_action is None?")

        # Call the method that will be overriden by subclasses of this
        chosen_action = self._choose_action(bundled_states)
        
        # Save the state and the action taken
        self.previous_state = state
        self.previous_action = chosen_action

        # Save animation state countdown
        player = self.game.gameData.player(self.player_id)
        if player.state.character_state is 1:
            self.last_animation_state_countdown = player.state.change_animation_state_countdown

class ObservingPlayer(BasePlayer):
    def __init__(self, player_id, game, exp_recorder, opts):
        super().__init__(player_id, game, exp_recorder, opts)
        game.setInputOverride(player_id, False)

    def _choose_action(self, bundled_states):
        # just observe what we're currently pressing
        # player_inputs can be called ONCE PER PLAYONCE CALL PER PLAYER THIS IS IMPORTANT
        inputs = self.game.gameData.player_inputs(self.player_id)
        current_action = [bool(elem) for elem in inputs]

        # It's possible they're pressing some "exclusive" buttons simultanously, check for this
        # and set them both to False (= not moving) if that is the case

        #self.log("Player [" + str(self.player_id) + "] pactio: " + str(current_action))
        # up/down exclusive
        if current_action[0] and current_action[1]:
            current_action[0] = current_action[1] = False

        # left/right exclusive
        if current_action[2] and current_action[3]:
            current_action[2] = current_action[3] = False

        # ok find the matching action in the list
        #self.log("Player [" + str(self.player_id) + "] action: " + str(current_action))
        action_id = action_to_id(current_action)
        if action_id is None:
            self.log("Unable to find matching action for", current_action)

        return current_action

class Player(BasePlayer):
    def __init__(self, player_id, game, learner, hotkeys, opts):
        super().__init__(player_id, game, learner.exp_recorder, opts)

        self.random_enabled = opts["random_enabled"]
        self.random_temperature = opts["random_temperature"]

        self.learner = learner
        self.hotkeys = hotkeys

        # put this somewhere better. I'm sure there's a better place for this. lol...
        self.max_recent_q = []

        game.setInputOverride(player_id, True)

    def _choose_action(self, bundled_states):
        # Choose an action to perform this step
        chosen_action = None
        action_size = len(_id_to_action[0])

        predicted_q = self.learner.predict_q(bundled_states)
        
        '''
        # Map action id to values
        predictions = {}
        for i in range(0, len(predicted_q)):
            predictions[predicted_q[i]] = i

        # Note most of this could be replaced with one argmax, but I wanted to grab the top 10 for some other experimental code
        # Optimize it eventually....
        global_min_to_act = 0.2
        sorted_predicted_q = sorted(predicted_q, reverse=True)

        self.max_recent_q.append(sorted_predicted_q[0])
        if len(self.max_recent_q) > 10:
            self.max_recent_q.pop(0)

        upper_half = sorted_predicted_q[0:math.floor(len(sorted_predicted_q)/2.0)]

        #
        #self.log(" == top ==")
        #for i in range(0, 10):
        #    self.log(i, ": ", sorted_predicted_q[i])

        # this constant multiplied by the recent max q seems to almost be a passiveness setting
        # lower values = more random-looking actions, higher values = bouldering bot (stand still and hit it)
        if (sorted_predicted_q[0] < 0.8 * np.max(self.max_recent_q) or sorted_predicted_q[0] < global_min_to_act):
            move_qs = []
            for i in _move_only_actions:
                move_qs.append(predicted_q[i])
            # Pick the best movement only action
            chosen_action = np.max(move_qs)
            # convert q value to action id
            chosen_action = predictions[chosen_action]
        # Either sample a random action or choose the best one
        '''
        if self.random_enabled:
            chosen_action = np.random.choice(self.action_size, p=softmax(predicted_q, self.random_temperature))
        else:
            chosen_action = np.argmax(predicted_q)

        self._apply_action(chosen_action)

        return chosen_action

    def _apply_action(self, action):
        '''
        Presses the hotkeys corresponding to the passed action for player
        '''
        action_hotkeys = id_to_action(action)

        for hotkey_id, hotkey in enumerate(self.hotkeys):
            self.game.setInputImmediate(hotkey, action_hotkeys[hotkey_id])

class ReinforcementLearner:
    """
    Predicts Q values and learns from experiences
    """

    def __init__(self, exp_recorder, batch_size, state_size, action_size, learn_rate, discount_factor, dimensionality):
        self.batch_size = batch_size
        self.state_size = state_size
        self.action_size = action_size
        
        self.exp_recorder = exp_recorder

        self.discount_factor = discount_factor

        # Build the model
        self.model = Sequential()

        step_1 = dimensionality
        step_2 = ((dimensionality - action_size) * 0.75) + action_size
        step_3 = action_size

        self.model.add(Dense(output_dim=step_1, input_dim=state_size))
        # expand outwards and then narrow
        self.model.add(PReLU())
        self.model.add(Dropout(0.05))
        self.model.add(Dense(input_dim=step_1, output_dim=step_2))
        self.model.add(PReLU())
        self.model.add(Dense(input_dim=step_2, output_dim=step_3))
        self.model.add(Activation('linear'))

        self.model.compile(loss="mse", optimizer=Adam())#lr=learn_rate))

        # The creator is responsible for calling load weights and load experiences

        self.last_files = []

    def predict_q(self, state):
        '''
        Predict the Q values for a single state
        '''

        return self.model.predict(state.reshape(1, self.state_size))[0]

    def load(self, path):
        '''
        Loads the models weights
        '''
        if os.path.exists(path):
            print("Loading weights from", path)
            self.model.load_weights(path)

    def save(self, path):
        '''
        Saves the models weights
        '''
        self.model.save_weights(path, True)

    def experience_replay(self):
        '''
        Perform a single experience replay learning step
        '''

        # Train on all data
        chosen_experiences = self.exp_recorder.get_random_experiences(100 * self.batch_size)
        if not chosen_experiences:
            print("Not enough experiences yet (" + str(self.exp_recorder.experience_count()) + "/" + str(100 * self.batch_size) + ")")
            return False

        avg_loss = 0
        for k, experiences in enumerate([chosen_experiences[i*self.batch_size:(i+1)*self.batch_size] for i in range(100)]):
            # Put all experiences data into numpy arrays
            states = np.array([ex.state for ex in experiences])
            rewards = np.array([ex.reward for ex in experiences])
            new_states = np.array([ex.new_state for ex in experiences])

            # First predict the Q values for the old state, then predict
            # the Qs of the new state and set the actual Q of the chosen
            # action for each batch
            actual_qs = self.model.predict(states)
            predicted_new_qs = self.model.predict(new_states)

            for i in range(len(experiences)):
                chosen_action = experiences[i].action
                actual_qs[i][chosen_action] = self.discount_factor * predicted_new_qs[i][chosen_action] + rewards[i] if not experiences[i].terminal else experiences[i].reward

            avg_loss += np.mean(self.model.train_on_batch(states, actual_qs))
            sys.stdout.write("\rAvg Loss: " + str(avg_loss / (k+1)))
            sys.stdout.flush()

        print("\nFinished one epoch")

        return True