PlayLethalLeague
================

![](http://i.imgur.com/1Pd5Q2u.png)

This is a project by @paralin and @toraxxx to train a neural network to play the video game "Lethal League".

It uses DLL injection, code caving, instruction-level detouring, and other techniques to inject code into the game to retreive pointers to relevant datastructures such as player and entity positions, velocities, and other states. Furthermore, a python intrepreter is embedded into the game, and used to load Keras and Theano.

This allows for a very easy to use python environment to develop AI for the game, with easy access to current game state and input overrides.

The current code uses experience learning, Q learning, and a sequential net with Keras to learn / play the game.

Run `offline_learning.py` to learn, and inject into the game to get experiences and test the `weights.dat` the learning produces.
