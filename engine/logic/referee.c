#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "referee.h"
#include "game/possession.h"
#include "entities/team.h"

/**
 * @brief Determines whether a goal has been scored.
 *
 * This function checks if the ball has completely crossed either goal line
 * while also being fully inside the vertical goal mouth.
 *
 * Important details:
 * - A goal is only valid if the *entire ball* (taking BALL_RADIUS into account)
 *   has crossed the goal line.
 * - The ball must be vertically contained between the goal posts.
 * - The right goal corresponds to Team 1 scoring.
 * - The left goal corresponds to Team 2 scoring.
 * @return
 * - 1 if Team 1 scores,
 * - 2 if Team 2 scores,
 * - 0 if no goal has occurred.
 */
static int goal(float x, float y) {
    float left_line = PITCH_X;
    float right_line = PITCH_X + PITCH_W;
    float goal_top = CENTER_Y - GOAL_HEIGHT / 2.0f;
    float goal_bottom = CENTER_Y + GOAL_HEIGHT / 2.0f;

    bool inside_goal_mouth =
        (y - BALL_RADIUS >= goal_top) &&
        (y + BALL_RADIUS <= goal_bottom);

    if (inside_goal_mouth && (x - BALL_RADIUS > right_line)) {
        printf("GOAL! Right net hit at x:%.2f, y=%.2f\n", x, y);
        return 1;
    }

    if (inside_goal_mouth && (x + BALL_RADIUS < left_line)) {
        printf("GOAL! Left net hit at x:%.2f, y=%.2f\n", x, y);
        return 2;
    }

    return 0;
}

/**
 * @brief Checks whether the ball is out of bounds.
 *
 * The ball is considered out only when its *entire area* lies outside
 * the pitch boundaries. Partial overlap with the pitch does NOT count as out.
 *
 * Notes for students:
 * - Use BALL_RADIUS to ensure the whole ball has crossed a boundary.
 * - All four pitch sides (left, right, top, bottom) must be considered.
 * - This function does not handle goals; goal detection is performed separately.
 * @return true if the ball is fully out of bounds, false otherwise.
 */
static bool out(float x, float y) {
    float left_line = PITCH_X;
    float right_line = PITCH_X + PITCH_W;
    float top_line = PITCH_Y;
    float bottom_line = PITCH_Y + PITCH_H;

    bool out_left = x + BALL_RADIUS < left_line;
    bool out_right = x - BALL_RADIUS > right_line;
    bool out_top = y + BALL_RADIUS < top_line;
    bool out_bottom = y - BALL_RADIUS > bottom_line;

    if (out_left || out_right || out_top || out_bottom) {
        printf("Ball is out: x=%.2f, y=%.2f\n", x, y);
        return true;
    }

    return false;
}

/**
 * @brief Acts as the game referee for one simulation step.
 *
 * This function is responsible for detecting and resolving game events
 * related to the ball, such as goals and out-of-bounds situations.
 *
 * Responsibilities:
 * - Check for goals BEFORE checking for out-of-bounds.
 * - Update team scores if a goal is detected.
 * - Report the appropriate game event.
 *
 * Notes for students:
 * - A goal must be checked first because a scored goal is technically out.
 * - If no event occurs, the game continues normally.
 *
 * @param scene Pointer to the current game scene.
 *
 * @return
 * - GOAL if a goal has been scored,
 * - OUT if the ball is out of bounds,
 * - 0 if no event occurred.
 */
int referee(struct Scene* scene) {
    float x = scene->ball->position.x;
    float y = scene->ball->position.y;

    int scored = goal(x, y);
    if (scored == 1) {
        scene->first_team->score += 1;
        return GOAL;
    }
    if (scored == 2) {
        scene->second_team->score += 1;
        return GOAL;
    }

    if (out(x, y))
        return OUT;

    return PLAY_ON;
}


/**
 * @brief Verifies the validity of a player's talent distribution.
 *
 * This function checks whether each individual talent value is within
 * the allowed range and whether the total talent points do not exceed
 * the maximum allowed per player.
 *
 * Notes for students:
 * - Each skill must be between 1 and MAX_TALENT_PER_SKILL (inclusive).
 * - The sum of all skills must not exceed MAX_TALENT_PER_PLAYER.
 * - Invalid configurations should be reported as errors.
 *
 * @param talents The talent structure to validate.
 */
void verify_talents(struct Talents talents) {
    int sum = talents.defence + talents.agility + talents.dribbling + talents.shooting;

    bool invalid =
        talents.defence < 1 || talents.defence > MAX_TALENT_PER_SKILL ||
        talents.agility < 1 || talents.agility > MAX_TALENT_PER_SKILL ||
        talents.dribbling < 1 || talents.dribbling > MAX_TALENT_PER_SKILL ||
        talents.shooting < 1 || talents.shooting > MAX_TALENT_PER_SKILL ||
        sum > MAX_TALENT_PER_PLAYER;

    if (invalid) {
        printf("ERROR: Invalid talents! Values: defence=%d, agility=%d, dribbling=%d, shooting=%d, sum=%d\n",
            talents.defence, talents.agility, talents.dribbling, talents.shooting, sum);
    }
}


/**
 * @brief Verifies the correctness of a player's current state.
 *
 * Ensures that the player's state is consistent with the game rules.
 * In particular, only the player who currently possesses the ball
 * is allowed to be in the SHOOTING state.
 *
 * Notes for students:
 * - If a player attempts to shoot without possessing the ball,
 *   their state must be corrected.
 *
 * @param player Pointer to the player being verified.
 * @param scene  Pointer to the current game scene.
 */
void verify_state(struct Player *player, struct Scene *scene) {
    if (scene->ball->possessor != player && player->state == SHOOTING) {
        printf(" ERROR: the ball is not yours, you can't shoot! (team %d, player %d)\n",
                player->team, player->kit);
        player->state = MOVING;
    }
}

/**
 * @brief Verifies and limits a player's movement speed.
 *
 * This function ensures that a player's velocity does not exceed
 * the maximum allowed speed derived from their agility talent.
 *
 * Notes for students:
 * - Maximum speed scales linearly with the agility talent.
 * - Both x and y velocity components must be checked independently.
 * - If a component exceeds the limit, it must be clamped.
 *
 * @param player Pointer to the player whose movement is being verified.
 */
void verify_movement(struct Player *player) {
    float max = ((float)player->talents.agility / MAX_TALENT_PER_SKILL) * MAX_PLAYER_VELOCITY;

    if (fabsf(player->velocity.x) > max) {
        printf(" ERROR: Demanding to run too fast in dimension x! (team %d, player %d)\n", player->team, player->kit);
        player->velocity.x = (player->velocity.x > 0.0f) ? max : -max;
    }

    if (fabsf(player->velocity.y) > max) {
        printf(" ERROR: Demanding to run too fast in dimension y! (team %d, player %d)\n", player->team, player->kit);
        player->velocity.y = (player->velocity.y > 0.0f) ? max : -max;
    }
}

/**
 * @brief Verifies the validity of a ball shot.
 *
 * This function ensures that the ball's velocity after a shot does not
 * exceed the maximum allowed speed derived from the shooter's talent.
 *
 * Additional kickoff rules:
 * - During kickoff, the ball must be played into the player's own half.
 *
 * Notes for students:
 * - Ball velocity must be clamped if it exceeds the allowed maximum.
 * - Both velocity components must be checked independently.
 *
 * @param ball    Pointer to the ball being shot.
 * @param kickoff True if the shot occurs during kickoff.
 */
void verify_shoot(struct Ball *ball, bool kickoff) {
    struct Player *player = ball->possessor;
    if (!player)
        return;

    float max = MAX_BALL_VELOCITY * ((float)player->talents.shooting / MAX_TALENT_PER_SKILL);

    if (fabsf(ball->velocity.x) > max) {
        printf(" ERROR: Demanding to shoot too fast in dimension x! (team %d, player %d)\n", player->team, player->kit);
        ball->velocity.x = (ball->velocity.x > 0.0f) ? max : -max;
    }

    if (fabsf(ball->velocity.y) > max) {
        printf(" ERROR: Demanding to shoot too fast in dimension y! (team %d, player %d)\n", player->team, player->kit);
        ball->velocity.y = (ball->velocity.y > 0.0f) ? max : -max;
    }

    if (kickoff) {
        bool invalid_team1 = (player->team == 1) && (ball->velocity.x > 0.0f);
        bool invalid_team2 = (player->team == 2) && (ball->velocity.x < 0.0f);
        if (invalid_team1 || invalid_team2)
            printf(" ERROR: You must pass to your own half! (team %d, player %d)\n", player->team, player->kit);
    }
}
