#pragma once

// Structs referring to data structures in the game

struct DevRegion
{
	char pad0[0x123];
	char hitboxes;
	char frameAdvance;
	char pad1[0x1];
	char windowActive;
};

struct BallState
{
	char pad0[0x10];
	// + 10
	int xspeed;
	// + 14
	int yspeed;
	char pad1[0x04];
	// 1c
	char hitstun;
	// +1d
	char ballExists;
	char pad2[0x10A];
	int hitstunCooldown;
	int ballSpeed;
	char ballTag;
	char pad3[0x03];
	char ballTagVisual;
	char pad4[0x7];
	// + 13c
	int xspeed_b;
	int yspeed_b;
	int state;
	char pad5[0xC];
	// +154
	char direction;
	char pad6[0x03];
	int hitCount;
	char pad7[0x1C];
	int serveResetCounter;
	int serveLowerCounter;
};

struct EntityCoords
{
	char pad0[0x18];
	int xcoord;
	int ycoord;
};

struct EntityBase
{
	char pad0[0x28];
	int fall_speed;
	float graphics_offset_x;
	float graphics_offset_y;
	float xcoord_readonly;
	float ycoord_readonly;
	char pad1[0x3C];
	float xcoord2_read_only;
	float ycoord2_read_only;
	char pad2[0xFC];
	int jump_height;
	int hold_jump_fall_speed;
	int fast_fall_speed;
	char pad3[0x8];
	int ground_acceleration;
	int ground_deceleration;
	int crouch_deceleration;
	int air_deceleration;
	int air_acceleration;
	int max_speed;
	int is_standing_while_walking;
	intptr_t neutral_1_hitbox;
	intptr_t neutral_2_hitbox;
	intptr_t neutral_3_hitbox;
	intptr_t neutral_4_hitbox;
	int neutral_5_recovery; // and bunt_4_recovery
	int neutral_6_recovery;
	char pad4[0x4];
	int charge_length;
	intptr_t smash1_front_hitbox; // and tophitbox
	intptr_t smash2_front_hitbox; // and tophitbox
	intptr_t smash3_front_hitbox; // and fronthitbox / 3-hitbox
	intptr_t smash4_recovery_hitbox;
	intptr_t bunt1_hitbox;
	char pad5[0x14];
	int crouch_transition; // and crouchturn
	int uncrouch_transition;
	int uncrouch_delay;
	char pad6[0x04];
	int taunt_1;
	int taunt_2;
	int expression_movement_cooldown;
	char pad7[0x14];
	int up_angle;
	int ground_down_angle;
	int air_down_angle;
	int smash_angle;
};

struct PlayerState
{
	char pad0[0x04];
	char facing_direction;
	char pad1[0xB];
	int horizontal_speed;
	int vertical_speed;
	char pad2[0x5];
	char exists;
	char touching_ground;
	char touching_right_wall;
	char touching_left_wall;
	char touching_ceiling;
	char pad3[0x112];
	int character_state;
	int animation_state;
	int hitstunCooldown;
	int change_animation_state_countdown;
	int maybe_cant_move_cooldown;
	int maybe_nothing;
	int respawn_timer;
	int special_meter;
	int charge;
	char pad5[0x8];
	char in_hitstun;
	char special_activated;
	char state_not_zero;
	char has_attacked_once;
	char pad6[0x8];
	int lives;
	char pad7[0x8];
	int total_hit_counter;
	int bunt_counter;
	char pad8[0x14];
	char hitbox_hit_ball;
};

struct HitboxCoords
{
	int xcoord;
	int ycoord;
	char pad0[0x10];
	int xorigin;
	int yorigin;
};

struct HitboxInfo
{
	char active;
	char pad0[0x13];
	int width;
	int height;
	char pad1[0x134];
	intptr_t coords_ptr;
};

struct Hitbox 
{
	HitboxCoords coords;
	HitboxInfo info;
};

struct Stage
{
	char pad0[0x1c];
	int x_origin;
	int y_origin;
	int x_size;
	int y_size;
};

// inputs
// by bit for first byte
//  7: taunt - 0x80
//  6: jump - 0x40
//  5: bunt - 0x20
//  4: attack - 0x10
//  3: left - 0x08
//  2: down - 2^2 = 0x04
//  1: right - 2^1 = 0x02
//  0: up - 2^0 = 0x01


#define CONTROL_UP 0x01
#define CONTROL_RIGHT 0x02
#define CONTROL_DOWN 0x04
#define CONTROL_LEFT 0x08
#define CONTROL_ATTACK 0x10
#define CONTROL_BUNT 0x20
#define CONTROL_JUMP 0x40
#define CONTROL_TAUNT 0x80