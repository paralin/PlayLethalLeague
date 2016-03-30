import imp
from neural import LethalInterface

class LethalInterfaceReloader:
    def reload(self):
        imp.reload(neural)