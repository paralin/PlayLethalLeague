import lethalai as la
import time
from lloptions import options

class OfflineTrainer:
    def __init__(self, exp_recorder):
        self.learner = la.ReinforcementLearner(exp_recorder, options["batch_size"], options["state_count"] * options["state_size"], options["action_size"],
                options["learn_rate"], options["discount_factor"], options["dimensionality"])

        self.learner.load("weights.dat")
    
    def train(self):
        '''
        Performs a single batch of experience replay

        Returns: True if learning was performed, False otherwise
        '''
        if not self.learner.experience_replay():
            return False
        
        return True

    def save_weights(self):
        self.learner.save("weights.dat")

if __name__ == "__main__":
    exp_recorder = la.ExperienceRecorder(options["state_size"] * options["state_count"])
    exp_recorder.check_new_experiences()
    trainer = OfflineTrainer(exp_recorder)
    print("Starting training.")
    learn_count = 0

    while True:
        try:
            exp_recorder.check_new_experiences()
            if trainer.train():
                learn_count += 1
                if learn_count % options["save_interval"] == 0:
                    print("Saving weights")
                    trainer.save_weights()
            else:
                time.sleep(1)

        except KeyboardInterrupt:
            break
