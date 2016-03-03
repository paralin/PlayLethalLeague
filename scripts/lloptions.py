import lethalai as la
options = {
    "state_size": 16,
    "action_size": la.get_action_count(),
    "state_count": 1,
    "random_enabled": True,
    "random_temperature": 0.92, # Low: All actions equally likely, High: Higher Q more likely
    "learn_rate": 0.002,
    "discount_factor": 0.95,
    "dimensionality": 300,
    "update_interval": 20,
    "experience_dump_min": 64,
    "disable_experience_dump": False,
    "batch_size": 256,
    "save_interval": 5,
}