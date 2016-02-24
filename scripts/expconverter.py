import lethalai as la
import fnmatch
import os
import pickle

class Experience:
    def __init__(self, state, action, reward, new_state, terminal):
        self.state = state
        self.action = action
        self.reward = reward
        self.new_state = new_state
        self.terminal = terminal

if __name__ == "__main__":
    matches = []
    matches_filenames = {}
    for root, dirnames, filenames in os.walk('oldexperiences'):
        for filename in fnmatch.filter(filenames, '*.exp'):
            path = os.path.join(root, filename)
            matches.append(path)
            matches_filenames[path] = filename
    
    for i, match in enumerate(matches):
        new_exps = []
        with open(match, "rb") as f:
            exps = pickle.load(f)

            for e in exps:
                la_exp = la.Experience(e.state, e.action, e.reward, e.new_state, e.terminal)
                new_exps.append(la_exp)

        with open("experiences/" + matches_filenames[match], "wb") as f:
            pickle.dump(new_exps, f)

        if i % 10 == 0:
            print("Converted", i, "of", len(matches))

    print("Done")
