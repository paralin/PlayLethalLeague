import numpy as np
from neural import ReinforcementLearner, LethalInterface

class TestNeural():
    def test_learner(self):
        state = np.ones((1, 10,))
        actual_qs = 2 * np.ones((1, 20,))
        learner = ReinforcementLearner(10, 20)
        print("Initialized learner")

        # Should converge on predicting avg 2
        for i in range(10):
            predicted_qs = learner.predict_q(state)
            learner.learn(state, actual_qs)
            post_predicted_qs = learner.predict_q(state)

            print(i, "Avg predicted:", np.mean(predicted_qs), "Avg post-learn predicted:", np.mean(post_predicted_qs))

    def test_interface(self):
        interface = LethalInterface()
        print("Initialized interface")

        interface.newMatchStarted()
        print("Called newMatchStarted")

        for i in range(10):
            interface.playOneFrame()
            print("Played frame", i)

if __name__ == "__main__":
    neural = TestNeural()

    print("Testing interface")
    neural.test_interface()

    print("Testing learner")
    neural.test_learner()

    print("Done")
