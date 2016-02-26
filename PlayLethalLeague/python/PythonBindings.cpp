#include <boost/python.hpp>
#include <injection/Game.h>

int getTickCountAsInt()
{
	return static_cast<int>(GetTickCount());
}

void playLLLog(const char* str)
{
	// python adds newline automatically
	std::cout << str;
	// LOG(str);
}


#define DEFINE_CONSTANT(X) scope().attr(#X) = X
#define ADD_POINTER(TYPE, NAME) .add_property(#NAME, make_getter(&TYPE::NAME, return_internal_reference<>()))
BOOST_PYTHON_MODULE(LethalLeague)
{
	using namespace boost::python;
	def("get_tick_count", &getTickCountAsInt);
	def("log", &playLLLog);
	DEFINE_CONSTANT(CONTROL_UP);
	DEFINE_CONSTANT(CONTROL_RIGHT);
	DEFINE_CONSTANT(CONTROL_DOWN);
	DEFINE_CONSTANT(CONTROL_LEFT);
	DEFINE_CONSTANT(CONTROL_ATTACK);
	DEFINE_CONSTANT(CONTROL_BUNT);
	DEFINE_CONSTANT(CONTROL_JUMP);
	DEFINE_CONSTANT(CONTROL_TAUNT);
	class_<Game>("Game", no_init)
		.def("setInputsEnabled", &Game::setInputsEnabled)
		.def("setInputImmediate", &Game::setInputImmediate)
		.def("setInputOverride", &Game::setInputOverride)
		.def("holdInputUntil", &Game::holdInputUntil)
		.def("setPlayerLives", &Game::setPlayerLives)
		.def("setPlayerExists", &Game::setPlayerExists)
		.def("resetPlayerHitCounters", &Game::resetPlayerHitCounters)
		.def("resetPlayerBuntCounters", &Game::resetPlayerBuntCounters)
		.def("sendTaunt", &Game::sendTaunt)
		.def("resetInputs", &Game::resetInputs)
		.def("respawnPlayer", &Game::respawnPlayer)
		.def("resetBall", &Game::resetBall)
		ADD_POINTER(Game, gameData);
	class_<GameStorage, GameStorage*>("GameStorage", no_init)
		.def("player", &GameStorage::getSinglePlayer)
		.def("player_inputs", &GameStorage::getSinglePlayerInputs)
		.add_property("is_online", &GameStorage::isOnline)
		ADD_POINTER(GameStorage, ball_coord)
		ADD_POINTER(GameStorage, ball_state)
		ADD_POINTER(GameStorage, dev_base)
		ADD_POINTER(GameStorage, stage_base);
	class_<SinglePlayer>("SinglePlayer", no_init)
		ADD_POINTER(SinglePlayer, coords)
		ADD_POINTER(SinglePlayer, state)
		ADD_POINTER(SinglePlayer, base);
	class_<EntityCoords, EntityCoords*>("EntityCoords")
		.add_property("xcoord", &EntityCoords::xcoord)
		.add_property("ycoord", &EntityCoords::ycoord);
	class_<DevRegion, DevRegion*>("DevRegion")
		.add_property("hitboxes", &DevRegion::hitboxes)
		.add_property("frameAdvance", &DevRegion::frameAdvance)
		.add_property("windowActive", &DevRegion::windowActive);
	class_<BallState, BallState*>("BallState")
		.add_property("xspeed", &BallState::xspeed)
		.add_property("yspeed", &BallState::yspeed)
		.add_property("hitstun", &BallState::hitstun)
		.add_property("exists", &BallState::ballExists)
		.add_property("hitstunCooldown", &BallState::hitstunCooldown)
		.add_property("ballSpeed", &BallState::ballSpeed)
		.add_property("ballTag", &BallState::ballTag)
		.add_property("ballTagVisual", &BallState::ballTagVisual)
		.add_property("xspeed_b", &BallState::xspeed_b)
		.add_property("yspeed_b", &BallState::yspeed_b)
		.add_property("state", &BallState::state)
		.add_property("direction", &BallState::direction)
		.add_property("hitCount", &BallState::hitCount)
		.add_property("serveResetCounter", &BallState::serveResetCounter)
		.add_property("serveLowerCounter", &BallState::serveLowerCounter);
	class_<EntityBase, EntityBase*>("EntityBase")
		.add_property("fall_speed", &EntityBase::fall_speed)
		.add_property("graphics_offset_x", &EntityBase::graphics_offset_x)
		.add_property("graphics_offset_y", &EntityBase::graphics_offset_y)
		.add_property("xcoord_readonly", &EntityBase::xcoord_readonly)
		.add_property("ycoord_readonly", &EntityBase::ycoord_readonly)
		.add_property("xcoord2_readonly", &EntityBase::xcoord2_read_only)
		.add_property("ycoord2_readonly", &EntityBase::ycoord2_read_only)
		.add_property("jump_height", &EntityBase::jump_height)
		.add_property("hold_jump_fall_speed", &EntityBase::hold_jump_fall_speed)
		.add_property("fast_fall_speed", &EntityBase::fast_fall_speed)
		.add_property("ground_acceleration", &EntityBase::ground_acceleration)
		.add_property("ground_deceleration", &EntityBase::ground_deceleration)
		.add_property("crouch_deceleration", &EntityBase::crouch_deceleration)
		.add_property("air_deceleration", &EntityBase::air_deceleration)
		.add_property("air_acceleration", &EntityBase::air_acceleration)
		.add_property("max_speed", &EntityBase::max_speed)
		.add_property("is_standing_while_walking", &EntityBase::is_standing_while_walking)
		.add_property("charge_length", &EntityBase::charge_length)
		.add_property("crouch_transition", &EntityBase::crouch_transition)
		.add_property("uncrouch_transition", &EntityBase::uncrouch_transition)
		.add_property("uncrouch_delay", &EntityBase::uncrouch_delay)
		.add_property("taunt_1", &EntityBase::taunt_1)
		.add_property("taunt_2", &EntityBase::taunt_2)
		.add_property("expression_movement_cooldown", &EntityBase::expression_movement_cooldown)
		.add_property("up_angle", &EntityBase::up_angle)
		.add_property("ground_down_angle", &EntityBase::ground_down_angle)
		.add_property("air_down_angle", &EntityBase::air_down_angle)
		.add_property("smash_angle", &EntityBase::smash_angle);
	class_<PlayerState, PlayerState*>("PlayerState")
		.add_property("facing_direction", &PlayerState::facing_direction)
		.add_property("horizontal_speed", &PlayerState::horizontal_speed)
		.add_property("vertical_speed", &PlayerState::vertical_speed)
		.add_property("exists", &PlayerState::exists)
		.add_property("touching_ground", &PlayerState::touching_ground)
		.add_property("touching_right_wall", &PlayerState::touching_right_wall)
		.add_property("touching_left_wall", &PlayerState::touching_left_wall)
		.add_property("touching_ceiling", &PlayerState::touching_ceiling)
		.add_property("character_state", &PlayerState::character_state)
		.add_property("animation_state", &PlayerState::animation_state)
		.add_property("change_animation_state_countdown", &PlayerState::change_animation_state_countdown)
		.add_property("respawn_timer", &PlayerState::respawn_timer)
		.add_property("special_meter", &PlayerState::special_meter)
		.add_property("charge", &PlayerState::charge)
		.add_property("in_hitstun", &PlayerState::in_hitstun)
		.add_property("special_activated", &PlayerState::special_activated)
		.add_property("has_attacked_once", &PlayerState::has_attacked_once)
		.add_property("lives", &PlayerState::lives)
		.add_property("total_hit_counter", &PlayerState::total_hit_counter)
		.add_property("bunt_counter", &PlayerState::bunt_counter)
		.add_property("hitbox_hit_ball", &PlayerState::hitbox_hit_ball);
	class_<Stage, Stage*>("Stage")
		.add_property("x_origin", &Stage::x_origin)
		.add_property("y_origin", &Stage::y_origin)
		.add_property("x_size", &Stage::x_size)
		.add_property("y_size", &Stage::y_size);
}