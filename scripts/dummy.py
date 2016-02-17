class DummyCoord:
    def __init__(self):
        self.xcoord = 0
        self.ycoord = 0

class DummyGameData:
    def __init__(self):
        self.ball_coord = DummyCoord()
        self.player_coords = [DummyCoord(), DummyCoord()]

class DummyPlayerState:
    def __init__(self):
        self.total_hit_counter = 0

# Dummy class for testing
class DummyLethalLeague:
    CONTROL_UP = 0
    CONTROL_DOWN = 1
    CONTROL_LEFT = 2
    CONTROL_RIGHT = 3
    CONTROL_ATTACK = 4
    CONTROL_BUNT = 5
    CONTROL_JUMP = 6

    PlayerState = DummyPlayerState()
    GameData = DummyGameData()

    def log(msg):
        print("LethalLeague.log:", msg)

    def get_tick_count():
        return 0

    def setInputImmediate(button, enabled):
        print("Set input immediate", button, enabled)
