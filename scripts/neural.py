class LethalLeagueOutput:
    def write(self, txt):
        ll.log(txt)

    def flush(self):
        pass

import sys
try:
    import LethalLeague as ll
    sys.stdout = sys.stderr = LethalLeagueOutput()
except:
    from dummy import DummyLethalLeague as ll

def log(*txt):
    print("[Python] ", *txt)

import os.path
import time
import numpy as np
import importlib
import lethalai as la

# TODO: Allow these to be set in the command line
options = {
    "state_size": 16,
    "action_size": la.get_action_count(),
    "state_count": 1,
    "random_enabled": True,
    "random_temperature": 0.9, # Low: All actions equally likely, High: Higher Q more likely
    "learn_rate": 0.002,
    "discount_factor": 0.9,
    "dimensionality": 300,
    "update_interval": 20,
    "experience_dump_min": 64,
    "disable_experience_dump": False,
}

class LethalInterface:
    hotkeys = [
        ll.CONTROL_UP, ll.CONTROL_DOWN,
        ll.CONTROL_LEFT, ll.CONTROL_RIGHT,
        ll.CONTROL_ATTACK, ll.CONTROL_BUNT,
        ll.CONTROL_JUMP,
    ]

    def __init__(self, game, scripts_root):
        self.game = game
        self.new_match_called_at_least_once = False
        self.scripts_root = scripts_root
        
        self.experiences_path = scripts_root + "experiences/"
        if not os.path.exists(self.experiences_path):
            os.makedirs(self.experiences_path)
        
        self.currently_in_game = False

        self.exp_recorder = la.ExperienceRecorder(options["state_size"] * options["state_count"])

        # We want 1 batch size for the player
        self.learner = la.ReinforcementLearner(self.exp_recorder, 1, options["state_count"] * options["state_size"], options["action_size"],
                options["learn_rate"], options["discount_factor"], options["dimensionality"])

        log("Initialized ReinforcementLearner")

        # Load the weights
        self.weights_path = scripts_root + "weights.dat"
        if os.path.exists(self.weights_path):
            self.last_weight_mod_time = time.ctime(os.path.getmtime(self.weights_path))
            self.learner.load(self.weights_path)
        else:
            self.last_weight_mod_time = None

    def newMatchStarted(self):
        log("newMatchStarted called, reimporting LethalAI")
        importlib.reload(la)
        self.currently_in_game = True
        self.was_playing = False
        
        # Online detection
        is_online = False
        if ord(self.game.gameData.is_online) == 1 and False:
            is_online = True
            log("Detected online match")
        self.is_online = is_online
        
        # Create the new players to play or observe
        self.players = []
        for player_id in range(2):
            # Only create playing players for player 0 when not online
            if player_id == 0 and not is_online:
                self.players.append(la.Player(player_id, self.game, self.learner, LethalInterface.hotkeys, options))
            else:
                self.players.append(la.ObservingPlayer(player_id, self.game, self.exp_recorder, options))
        
        game_data = self.game.gameData

        self.base_lives = game_data.player(0).state.lives
        self.was_playing = False

        self.last_update_time = 0
        self.next_update_time = 0
        self.new_match_called_at_least_once = True

    def matchReset(self):
        self.currently_in_game = False

    def playOneFrame(self):
        now_ticks = ll.get_tick_count()
        if not self.new_match_called_at_least_once:
            return now_ticks + 10

        self.last_update_time = now_ticks
        self.next_update_time = now_ticks + options["update_interval"]

        # First check if we are actually in play
        game_data = self.game.gameData
        player_0 = game_data.player(0)
        player_1 = game_data.player(1)

        playing_now = game_data.ball_state.state != 14 and game_data.ball_state.state != 1

        if not playing_now and self.was_playing:
            log("== end of life ==")
            self.players[0].end_of_life()
            self.players[1].end_of_life()

            we_died = player_0.state.lives < player_1.state.lives
            winner_id = 1 if we_died else 0
            self.players[winner_id].update(30, True)
            self.players[winner_id].add_round_winner(True)
            self.players[1 - winner_id].update(-30, True)
            self.players[1 - winner_id].add_round_winner(False)

            if not self.is_online:
                self.game.respawnPlayer(1 - winner_id)
                self.game.resetBall()

            print("Win percentage:", self.players[0].get_win_percentage())
            
            # Dump experiences if there are more than than a set amount and it's not disabled
            if self.exp_recorder.experience_count() >= options["experience_dump_min"] and not options["disable_experience_dump"]:
                log("Dumped experiences.")
                self.exp_recorder.save_experiences(self.experiences_path + str(ll.get_tick_count()) + '.exp')
                self.exp_recorder.reset()
            
            # Check if the weights file has change
            if os.path.exists(self.weights_path):
                weight_mod_time = time.ctime(os.path.getmtime(self.weights_path))
                if self.last_weight_mod_time != weight_mod_time:
                    self.learner.load(self.weights_path)
                    self.last_weight_mod_time = weight_mod_time
                    log("Reloaded weights from file.")

        self.was_playing = playing_now
        # for now just bail out if we're not actually in play
        if not playing_now:
            if not self.is_online:
                self.game.setPlayerLives(0, self.base_lives)
                self.game.setPlayerLives(1, self.base_lives)
            return self.next_update_time

        # Calculate things like total swing count, etc
        self.players[0].update()
        self.players[1].update()

        return self.next_update_time

