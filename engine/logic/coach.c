#include "coach.h"
#include "core/constants.h"
#include "entities/ball.h"
#include "entities/team.h"
#include "game/scene.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

// Set to false to let the other team use their own logic (if you implement it)
// Set to true to test your logic on both teams
bool coach_both_teams = true;

static float max_player_speed(const struct Player *self) {
    return ((float)self->talents.agility / MAX_TALENT_PER_SKILL) * MAX_PLAYER_VELOCITY;
}

static float max_ball_speed(const struct Player *self) {
    return ((float)self->talents.shooting / MAX_TALENT_PER_SKILL) * MAX_BALL_VELOCITY;
}

static void move_towards_target(struct Player *self, float target_x, float target_y, float motivation) {
    float dx = target_x - self->position.x;
    float dy = target_y - self->position.y;
    float d = hypotf(dx, dy);

    if (d <= 0.001f) {
        self->velocity.x = 0.0f;
        self->velocity.y = 0.0f;
        return;
    }

    float max_v = max_player_speed(self) * motivation;
    self->velocity.x = (dx / d) * max_v;
    self->velocity.y = (dy / d) * max_v;
}

static bool player_ball_colliding(const struct Player *p, const struct Ball *b) {
    float dx = p->position.x - b->position.x;
    float dy = p->position.y - b->position.y;
    float rs = p->radius + b->radius;
    return (dx * dx + dy * dy) <= (rs * rs);
}

void pressing_movement(struct Player *self, struct Scene *scene, float motivation) {
    move_towards_target(self, scene->ball->position.x, scene->ball->position.y, motivation);
}

void attacking_movement(struct Player *self, struct Scene *scene, float motivation) {
    float diff = (self->team == 1) ? (PITCH_W / 2.0f) : -(PITCH_W / 2.0f);
    move_towards_target(self, CENTER_X + diff, CENTER_Y, motivation);
    (void)scene;
}

void gk_movement(struct Player *self, struct Scene *scene) {
    float x_target = (self->team == 1) ? (PITCH_X + PLAYER_RADIUS) : (PITCH_X + PITCH_W - PLAYER_RADIUS);
    float goal_top = CENTER_Y - GOAL_HEIGHT / 2.0f + BALL_RADIUS;
    float goal_bottom = CENTER_Y + GOAL_HEIGHT / 2.0f - BALL_RADIUS;
    float y_target = self->position.y;

    if (scene->ball->position.y >= goal_top && scene->ball->position.y <= goal_bottom)
        y_target = scene->ball->position.y;

    move_towards_target(self, x_target, y_target, 1.0f);
}

void shoot(struct Player *self, struct Scene *scene, float x) {
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    float min_y = CENTER_Y - GOAL_HEIGHT / 2.0f + BALL_RADIUS;
    float max_y = CENTER_Y + GOAL_HEIGHT / 2.0f - BALL_RADIUS;
    float y_selection = min_y + ((float)rand() / (float)RAND_MAX) * (max_y - min_y);

    float dx = x - self->position.x;
    float dy = y_selection - self->position.y;
    float d = hypotf(dx, dy);

    if (d <= 0.001f) {
        scene->ball->velocity.x = 0.0f;
        scene->ball->velocity.y = 0.0f;
        return;
    }

    float max_v = max_ball_speed(self);
    scene->ball->velocity.x = (dx / d) * max_v;
    scene->ball->velocity.y = (dy / d) * max_v;
}

void pass(struct Player *self, struct Player *receiver, struct Scene *scene) {
    float dx = receiver->position.x - self->position.x;
    float dy = receiver->position.y - self->position.y;
    float d = hypotf(dx, dy);

    if (d <= 0.001f) {
        scene->ball->velocity.x = 0.0f;
        scene->ball->velocity.y = 0.0f;
        return;
    }

    float pass_speed = max_ball_speed(self) * 0.85f;
    scene->ball->velocity.x = (dx / d) * pass_speed;
    scene->ball->velocity.y = (dy / d) * pass_speed;
}

void decide_kick(struct Player *self, struct Scene *scene) {
    struct Team *current_team = (self->team == 1) ? scene->first_team : scene->second_team;
    struct Player *leader = self;

    bool can_shoot = (self->team == 1)
        ? (self->position.x > CENTER_X + PITCH_W / 6.0f)
        : (self->position.x < CENTER_X - PITCH_W / 6.0f);

    if (can_shoot) {
        shoot(self, scene, (self->team == 1) ? (PITCH_X + PITCH_W) : PITCH_X);
        return;
    }

    for (int i = 0; i < PLAYER_COUNT; i++) {
        struct Player *candidate = current_team->players[i];
        if (!candidate || candidate == self)
            continue;

        bool better = (self->team == 1)
            ? (candidate->position.x > leader->position.x)
            : (candidate->position.x < leader->position.x);

        if (better)
            leader = candidate;
    }

    if (leader != self) {
        pass(self, leader, scene);
        return;
    }

    int random = rand() % PLAYER_COUNT;
    if (current_team->players[random] && current_team->players[random] != self)
        pass(self, current_team->players[random], scene);
    else
        shoot(self, scene, (self->team == 1) ? (PITCH_X + PITCH_W) : PITCH_X);
}

void change_stater(struct Player *self, struct Scene *scene) {
    self->state = MOVING;
    if (player_ball_colliding(self, scene->ball))
        self->state = INTERCEPTING;
    if (scene->ball->possessor == self)
        self->state = SHOOTING;
}

/* -------------------------------------------------------------------------
 * Logic Functions
 *  TODO 1: You must implement the following functions in Phase 2.
 *        Each player in each team has its own functions.
 *        You can add new functions, but are NOT ALLOWED to remove
 *        the existing functions or change their structure.
 * -------------------------------------------------------------------------
 * ⚠️ STUDENT RULES FOR PHASE 2:
 * You are restricted to modifying ONLY specific variables in each function:
 *
 * 1. MOVEMENT FUNCTIONS (movement_logic_X_Y):
 * Allowed: player->velocity
 * Goal:    Determine the direction and speed of movement.
 *
 * 2. SHOOTING FUNCTIONS (shooting_logic_X_Y):
 * Allowed: ball->velocity
 * Goal:    Determine the direction and power of the kick/pass.
 *
 * 3. CHANGE STATE FUNCTIONS (change_state_logic_X_Y):
 * Allowed: player->state
 * Goal:    Switch between IDLE, MOVING, SHOOTING, or INTERCEPTING.
 *
 * NOTE: Directly modifying any other attributes will be flagged as a violation.
 * Thank you for your attention to this matter!
 * ------------------------------------------------------------------------- */

/* Team 1 movement logic */
void movement_logic_1_0(struct Player *self, struct Scene *scene) {
    if (scene->ball->possessor && scene->ball->possessor->team != self->team) {
        if (self->team == 1)
            (scene->ball->position.x > CENTER_X) ? pressing_movement(self, scene, 1.0f) : pressing_movement(self, scene, 0.4f);
        else
            (scene->ball->position.x < CENTER_X) ? pressing_movement(self, scene, 1.0f) : pressing_movement(self, scene, 0.4f);
    } else {
        attacking_movement(self, scene, 1.0f);
    }
}

void movement_logic_1_1(struct Player *self, struct Scene *scene) {
    if (scene->ball->possessor && scene->ball->possessor->team != self->team)
        pressing_movement(self, scene, 1.0f);
    else
        attacking_movement(self, scene, 0.8f);
}

void movement_logic_1_2(struct Player *self, struct Scene *scene) {
    if (scene->ball->possessor && scene->ball->possessor->team != self->team) {
        if (self->team == 1)
            (scene->ball->position.x < CENTER_X) ? pressing_movement(self, scene, 1.0f) : pressing_movement(self, scene, 0.25f);
        else
            (scene->ball->position.x > CENTER_X) ? pressing_movement(self, scene, 1.0f) : pressing_movement(self, scene, 0.25f);
    } else {
        attacking_movement(self, scene, 0.5f);
    }
}

void movement_logic_1_3(struct Player *self, struct Scene *scene) {
    gk_movement(self, scene);
}

void movement_logic_1_4(struct Player *self, struct Scene *scene) {
    movement_logic_1_2(self, scene);
}

void movement_logic_1_5(struct Player *self, struct Scene *scene) {
    movement_logic_1_1(self, scene);
}

/* Team 2 movement logic */
void movement_logic_2_0(struct Player *self, struct Scene *scene) { movement_logic_1_0(self, scene); }
void movement_logic_2_1(struct Player *self, struct Scene *scene) { movement_logic_1_1(self, scene); }
void movement_logic_2_2(struct Player *self, struct Scene *scene) { movement_logic_1_2(self, scene); }
void movement_logic_2_3(struct Player *self, struct Scene *scene) { movement_logic_1_3(self, scene); }
void movement_logic_2_4(struct Player *self, struct Scene *scene) { movement_logic_1_4(self, scene); }
void movement_logic_2_5(struct Player *self, struct Scene *scene) { movement_logic_1_5(self, scene); }

/* Team 1 shooting logic */
void shooting_logic_1_0(struct Player *self, struct Scene *scene) {
    decide_kick(self, scene);
}
void shooting_logic_1_1(struct Player *self, struct Scene *scene) {
    decide_kick(self, scene);
}
void shooting_logic_1_2(struct Player *self, struct Scene *scene) {
    decide_kick(self, scene);
}
void shooting_logic_1_3(struct Player *self, struct Scene *scene) {
    decide_kick(self, scene);
}
void shooting_logic_1_4(struct Player *self, struct Scene *scene) {
    decide_kick(self, scene);
}
void shooting_logic_1_5(struct Player *self, struct Scene *scene) {
    decide_kick(self, scene);
}

/* Team 2 shooting logic */
void shooting_logic_2_0(struct Player *self, struct Scene *scene) { decide_kick(self, scene); }
void shooting_logic_2_1(struct Player *self, struct Scene *scene) { decide_kick(self, scene); }
void shooting_logic_2_2(struct Player *self, struct Scene *scene) { decide_kick(self, scene); }
void shooting_logic_2_3(struct Player *self, struct Scene *scene) { decide_kick(self, scene); }
void shooting_logic_2_4(struct Player *self, struct Scene *scene) { decide_kick(self, scene); }
void shooting_logic_2_5(struct Player *self, struct Scene *scene) { decide_kick(self, scene); }

/* Team 1 change_state logic */
void change_state_logic_1_0(struct Player *self, struct Scene *scene) { change_stater(self, scene); }
void change_state_logic_1_1(struct Player *self, struct Scene *scene) { change_stater(self, scene); }
void change_state_logic_1_2(struct Player *self, struct Scene *scene) { change_stater(self, scene); }
void change_state_logic_1_3(struct Player *self, struct Scene *scene) { change_stater(self, scene); }
void change_state_logic_1_4(struct Player *self, struct Scene *scene) { change_stater(self, scene); }
void change_state_logic_1_5(struct Player *self, struct Scene *scene) { change_stater(self, scene); }

/* Team 2 change_state logic */
void change_state_logic_2_0(struct Player *self, struct Scene *scene) { change_stater(self, scene); }
void change_state_logic_2_1(struct Player *self, struct Scene *scene) { change_stater(self, scene); }
void change_state_logic_2_2(struct Player *self, struct Scene *scene) { change_stater(self, scene); }
void change_state_logic_2_3(struct Player *self, struct Scene *scene) { change_stater(self, scene); }
void change_state_logic_2_4(struct Player *self, struct Scene *scene) { change_stater(self, scene); }
void change_state_logic_2_5(struct Player *self, struct Scene *scene) { change_stater(self, scene); }

/* -------------------------------------------------------------------------
 * Lookup tables for factory
 * ------------------------------------------------------------------------- */
static PlayerLogicFn team1_movement[6] = {
    movement_logic_1_0, movement_logic_1_1, movement_logic_1_2,
    movement_logic_1_3, movement_logic_1_4, movement_logic_1_5
};

static PlayerLogicFn team2_movement[6] = {
    movement_logic_2_0, movement_logic_2_1, movement_logic_2_2,
    movement_logic_2_3, movement_logic_2_4, movement_logic_2_5
};

static PlayerLogicFn team1_shooting[6] = {
    shooting_logic_1_0, shooting_logic_1_1, shooting_logic_1_2,
    shooting_logic_1_3, shooting_logic_1_4, shooting_logic_1_5
};

static PlayerLogicFn team2_shooting[6] = {
    shooting_logic_2_0, shooting_logic_2_1, shooting_logic_2_2,
    shooting_logic_2_3, shooting_logic_2_4, shooting_logic_2_5
};

static PlayerLogicFn team1_change_state[6] = {
    change_state_logic_1_0, change_state_logic_1_1, change_state_logic_1_2,
    change_state_logic_1_3, change_state_logic_1_4, change_state_logic_1_5
};

static PlayerLogicFn team2_change_state[6] = {
    change_state_logic_2_0, change_state_logic_2_1, change_state_logic_2_2,
    change_state_logic_2_3, change_state_logic_2_4, change_state_logic_2_5
};

/* -------------------------------------------------------------------------
 * Factory functions
 * ------------------------------------------------------------------------- */
PlayerLogicFn get_movement_logic(int team, int kit) {
    if (coach_both_teams) return team1_movement[kit];
    return (team == 1) ? team1_movement[kit] : team2_movement[kit];
}

PlayerLogicFn get_shooting_logic(int team, int kit) {
    if (coach_both_teams) return team1_shooting[kit];
    return (team == 1) ? team1_shooting[kit] : team2_shooting[kit];
}

PlayerLogicFn get_change_state_logic(int team, int kit) {
    if (coach_both_teams) return team1_change_state[kit];
    return (team == 1) ? team1_change_state[kit] : team2_change_state[kit];
}

/* -------------------------------------------------------------------------
 * TALENTS defence/agility/dribbling/shooting
 *  TODO 2: Replace these default values with your desired skill points.
 * ------------------------------------------------------------------------- */
/* Team 1 CF, CM, CB, GK, CB, CM*/
static struct Talents team1_talents[6] = {
    {1, 6, 6, 7},
    {3, 5, 7, 5},
    {7, 4, 3, 3},
    {7, 3, 3, 7},
    {7, 4, 3, 3},
    {3, 6, 6, 5},
};

/* Team 2 */
static struct Talents team2_talents[6] = {
    {1, 6, 6, 7},
    {3, 5, 7, 5},
    {7, 4, 3, 3},
    {7, 3, 3, 7},
    {7, 4, 3, 3},
    {3, 6, 6, 5},
};

struct Talents get_talents(int team, int kit) {
    if (coach_both_teams) return team1_talents[kit];
    return (team == 1) ? team1_talents[kit] : team2_talents[kit];
}


/* -------------------------------------------------------------------------
 * Positioning
 *  TODO 3: Decide players positions at kick-off.
 *        Players must stay on their half, outside the center circle.
 *        Keep in mind that the kick-off team's first player will automatically
 *             be placed at the center of the pitch.
 * ------------------------------------------------------------------------- */
/* Team 1: CF, CM, CB, GK, CB, CM */
static struct Vec2 team1_positions[6] = {
    {300, CENTER_Y},
    {250, CENTER_Y-150},
    {200, CENTER_Y-75},
    {PITCH_X+PLAYER_RADIUS, CENTER_Y},
    {200, CENTER_Y+75},
    {250, CENTER_Y+150},
};

/* Team 2 */
static struct Vec2 team2_positions[6] = {
    {750, CENTER_Y},
    {800, CENTER_Y-150},
    {850, CENTER_Y-75},
    {PITCH_X+PITCH_W-PLAYER_RADIUS, CENTER_Y},
    {850, CENTER_Y+75},
    {800, CENTER_Y+150},
};


struct Vec2 get_positions(int team, int kit) {
    return (team == 1) ? team1_positions[kit] : team2_positions[kit];
}
