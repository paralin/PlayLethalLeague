Background Batch Concept
========================

This code restructure will do a few things to allow a background thread to learn on a massive batch size, with a target timing of around 6-10 seconds per learning.

The idea is to do this learning and then copy the resultant weights over to the live playing instance.

Changes to do this:

  - Two ReinforcementLearner objects with separate experience arrays.
  - A function called "applyLearning" which:
   - Moves the new experiences from the player over to the learner
   - Copies the weights from the learner back to the player
