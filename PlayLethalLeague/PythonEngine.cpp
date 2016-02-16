#include "stdafx.h"
#include "Game.h"
#include "PythonEngine.h"
#include <boost/python.hpp>
#include <Shlwapi.h>


PythonEngine::PythonEngine(Game* game, std::string scriptsRoot)
{
	this->game = game;
	this->scriptsRoot = scriptsRoot;
}

int getTickCountAsInt()
{
	return static_cast<int>(GetTickCount());
}

void playLLLog(const char* str)
{
	LOG(str);
}

BOOST_PYTHON_MODULE(LethalLeague)
{
	using namespace boost::python;
	def("get_tick_count", getTickCountAsInt);
	def("log", playLLLog);
	def("CONTROL_UP", CONTROL_UP);
	def("CONTROL_RIGHT", CONTROL_RIGHT);
	def("CONTROL_DOWN", CONTROL_DOWN);
	def("CONTROL_LEFT", CONTROL_LEFT);
	def("CONTROL_ATTACK", CONTROL_ATTACK);
	def("CONTROL_BUNT", CONTROL_BUNT);
	def("CONTROL_JUMP", CONTROL_JUMP);
	def("CONTROL_TAUNT", CONTROL_TAUNT);
	class_<Game>("Game")
		.def("setInputsEnabled", &Game::setInputsEnabled)
		.def("setInputImmediate", &Game::setInputImmediate)
		.def("holdInputUntil", &Game::holdInputUntil)
		.def("setPlayerLives", &Game::setPlayerLives)
		.def("setPlayerExists", &Game::setPlayerExists)
		.def("resetPlayerHitCounters", &Game::resetPlayerHitCounters)
		.def("resetPlayerBuntCounters", &Game::resetPlayerBuntCounters)
		.def("sendTaunt", &Game::sendTaunt)
		.def("resetInputs", &Game::resetInputs)
		.def_readonly("gameData", &Game::gameData);
	class_<GameStorage>("GameData")
		.def("ball_coord", &GameStorage::ball_coord)
		.def("ball_state", &GameStorage::ball_state)
		.def("dev_base", &GameStorage::dev_base)
		.def("stage_base", &GameStorage::stage_base)
		.def("player_bases", &GameStorage::stage_base)
		.def("player_coords", &GameStorage::player_coords)
		.def("player_states", &GameStorage::player_states)
		.def("forcedInputs", &GameStorage::forcedInputs);
	class_<EntityCoords>("EntityCoords")
		.def("xcoord", &EntityCoords::xcoord)
		.def("ycoord", &EntityCoords::ycoord);
	class_<DevRegion>("DevRegion")
		.def("hitboxes", &DevRegion::hitboxes)
		.def("frameAdvance", &DevRegion::frameAdvance)
		.def("windowActive", &DevRegion::windowActive);
	class_<BallState>("BallState")
		.def("xspeed", &BallState::xspeed)
		.def("yspeed", &BallState::yspeed)
		.def("hitstun", &BallState::hitstun)
		.def("exists", &BallState::ballExists)
		.def("hitstunCooldown", &BallState::hitstunCooldown)
		.def("ballSpeed", &BallState::ballSpeed)
		.def("ballTag", &BallState::ballTag)
		.def("ballTagVisual", &BallState::ballTagVisual)
		.def("xspeed_b", &BallState::xspeed_b)
		.def("yspeed_b", &BallState::yspeed_b)
		.def("state", &BallState::state)
		.def("direction", &BallState::direction)
		.def("hitCount", &BallState::hitCount)
		.def("serveResetCounter", &BallState::serveResetCounter)
		.def("serveLowerCounter", &BallState::serveLowerCounter);
	class_<EntityBase>("EntityBase")
		.def("fall_speed", &EntityBase::fall_speed)
		.def("graphics_offset_x", &EntityBase::graphics_offset_x)
		.def("graphics_offset_y", &EntityBase::graphics_offset_y)
		.def("xcoord_readonly", &EntityBase::xcoord_readonly)
		.def("ycoord_readonly", &EntityBase::ycoord_readonly)
		.def("xcoord2_readonly", &EntityBase::xcoord2_read_only)
		.def("ycoord2_readonly", &EntityBase::ycoord2_read_only)
		.def("jump_height", &EntityBase::jump_height)
		.def("hold_jump_fall_speed", &EntityBase::hold_jump_fall_speed)
		.def("fast_fall_speed", &EntityBase::fast_fall_speed)
		.def("ground_acceleration", &EntityBase::ground_acceleration)
		.def("ground_deceleration", &EntityBase::ground_deceleration)
		.def("crouch_deceleration", &EntityBase::crouch_deceleration)
		.def("air_deceleration", &EntityBase::air_deceleration)
		.def("air_acceleration", &EntityBase::air_acceleration)
		.def("max_speed", &EntityBase::max_speed)
		.def("is_standing_while_walking", &EntityBase::is_standing_while_walking)
		.def("charge_length", &EntityBase::charge_length)
		.def("crouch_transition", &EntityBase::crouch_transition)
		.def("uncrouch_transition", &EntityBase::uncrouch_transition)
		.def("uncrouch_delay", &EntityBase::uncrouch_delay)
		.def("taunt_1", &EntityBase::taunt_1)
		.def("taunt_2", &EntityBase::taunt_2)
		.def("expression_movement_cooldown", &EntityBase::expression_movement_cooldown)
		.def("up_angle", &EntityBase::up_angle)
		.def("ground_down_angle", &EntityBase::ground_down_angle)
		.def("air_down_angle", &EntityBase::air_down_angle)
		.def("smash_angle", &EntityBase::smash_angle);
	class_<PlayerState>("PlayerState")
		.def("facing_direction", &PlayerState::facing_direction)
		.def("horizontal_speed", &PlayerState::horizontal_speed)
		.def("vertical_speed", &PlayerState::vertical_speed)
		.def("exists", &PlayerState::exists)
		.def("touching_ground", &PlayerState::touching_ground)
		.def("touching_right_wall", &PlayerState::touching_right_wall)
		.def("touching_left_wall", &PlayerState::touching_left_wall)
		.def("touching_ceiling", &PlayerState::touching_ceiling)
		.def("character_state", &PlayerState::character_state)
		.def("animation_state", &PlayerState::animation_state)
		.def("respawn_timer", &PlayerState::respawn_timer)
		.def("special_meter", &PlayerState::special_meter)
		.def("charge", &PlayerState::charge)
		.def("in_hitstun", &PlayerState::in_hitstun)
		.def("special_activated", &PlayerState::special_activated)
		.def("has_attacked_once", &PlayerState::has_attacked_once)
		.def("lives", &PlayerState::lives)
		.def("total_hit_counter", &PlayerState::total_hit_counter)
		.def("bunt_counter", &PlayerState::bunt_counter)
		.def("hitbox_hit_ball", &PlayerState::hitbox_hit_ball);
	class_<Stage>("Stage")
		.def("x_origin", &Stage::x_origin)
		.def("y_origin", &Stage::y_origin)
		.def("x_size", &Stage::x_size)
		.def("y_size", &Stage::y_size);
}

PythonEngine::~PythonEngine()
{
}

bool PythonEngine::loadPythonCode()
{
	std::string expectedPath = scriptsRoot + "/neural.py";
	if (!PathFileExists(expectedPath.c_str()))
	{
		LOG("Script expected at: " << expectedPath << " but not found.");
		return false;
	}

	// boost::python::exec_file(expectedPath.c_str(), )
}
