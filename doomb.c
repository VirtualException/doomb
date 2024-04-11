#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SFML/Graphics.h>
#include "vec.h"

#define PI 3.141592653589793

/* Hard coded value. The sensitivity of the mouse for the current fps. */
#define MOUSE_DELTA_FACTOR 0.002

typedef struct _Wall {

        v2 a;           /* Start point of wall */
        v2 b;           /* End point of wall */

} Wall;

typedef struct _Camera {

        v2 pos;         /* Camera position */
        v2 dir;         /* Centered direction of the camera */
        double vel;     /* Camera velocity */
        double fov;     /* Field of view (radians) */

} Camera;


sfRenderWindow *rwindow = NULL;
sfView *view = NULL;
sfClock *clock = NULL;

sfVector2u winsize;
sfVector2i mouse;

/* TODO: Fix the window size dependent code */
const sfVector2u winsize_render = { 1600, 900 };

sfTexture *wall_tex = NULL;
sfRenderStates wall_render_state;
sfVertex wall_points[6];
sfVector2u wall_tex_size;
sfTexture* floor_tex;
sfRectangleShape* floor_rect;
sfRectangleShape* cursor;

Camera cam;

double dtime;

/* Wall tranformation, the responable of the 3D effect. */
float wall_trans_mat[9] = { /* aka 'sfTransform' */
        1.f, 0.f, 0.f,
        0.f, 1.f, 0.f,
        0.f, 0.f, 1.f
};

/* Basically, the map itself */
Wall *walls = NULL;
size_t walls_n = 0;


int wall_compare(Wall* w1, Wall *w2);
void render_wall(float a_x, float a_h, float b_x, float b_h, float padd);
void render();
void handle_events();


/* Used by qsort()'s __compar_fn_t function */
int
wall_compare(Wall* w1, Wall *w2)
{
        double w1dist = v2_distance(cam.pos, v2_mid(w1->a, w1->b));
        double w2dist = v2_distance(cam.pos, v2_mid(w2->a, w2->b));

        if (w1dist < w2dist)
                return 1;
        if (w1dist > w2dist)
                return -1;
        return 0;
}

/**
 * Renders a wall in 2d representation with 4 parameters thar are respectively:
 * First point in the horizontal line.
 * First point's heigth from the horizontal line.
 * Second point in the horizontal line.
 * Second point's heigth from the horizontal line.
*/

void
render_wall(float a_x, float a_h, float b_x, float b_h, float padd)
{
        /**
         * We start with a unit square in 2d. This coordinates will serve as
         * guide to the renderer to complete a tranformation, applying a 3x3
         * matrix. The evaluated coordinates (that we already know) will result
         * in a perspective deformed square, transformed by a given matrix.
         *
         * [T] is a trasformation from a unit square to an arbitrary isosceles
         * trapezoid layed down.
         * [T] is represented as a 3x3 matrix, and has 5 values we have to
         * find.
         * So, we find the values Xn so that [T(Xn)] * UNIT_SQUARE = TRAPEZOID
         *
         * The points we want to solve the transformation for (the final
         * trapezoid) are the following.
         * Keep in mind that they are NOT translated in the y coordinate.
         *
         * A' = (a_x, -a_h)
         * B' = (b_x, -b_h)
         * C' = (a_x, a_h)
         * D' = (b_x, b_h)
         *
         * Thanks to Javier Barja, my Algebra teacher, to whom i owe everything.
        */

        /* The solution. Matrix components. */
        float ma, mc, me, mf, mg;
        ma = b_x * a_h / b_h - a_x;
        mc = a_x;
        me = a_h - -a_h; /* + */
        mf = -a_h;
        mg = a_h / b_h - 1.f;

        /* Construction the matrix. Perspective tranformation. */
        wall_trans_mat[0] = ma;
        wall_trans_mat[2] = mc;
        wall_trans_mat[4] = me;
        wall_trans_mat[5] = mf;
        wall_trans_mat[6] = mg;

        /**
         * Edit texture area, so that, when clipping, only the visible area is
         * rendered. It does not matter what padding value is used; every time
         * tex cords are computed, so that no reset it needed.
        */

        if (padd > 0.f) {
                wall_points[0].texCoords.x = 0.f;
                wall_points[1].texCoords.x = 0.f;
                wall_points[2].texCoords.x = wall_tex_size.x * padd;
                wall_points[3].texCoords.x = 0.f;
                wall_points[4].texCoords.x = wall_tex_size.x * padd;
                wall_points[5].texCoords.x = wall_tex_size.x * padd;
        }
        else {
                wall_points[0].texCoords.x = wall_tex_size.x * (1+padd);
                wall_points[1].texCoords.x = wall_tex_size.x * (1+padd);
                wall_points[2].texCoords.x = wall_tex_size.x;
                wall_points[3].texCoords.x = wall_tex_size.x * (1+padd);
                wall_points[4].texCoords.x = wall_tex_size.x;
                wall_points[5].texCoords.x = wall_tex_size.x;
        }

        /* For some reason, the transform argument is lost in every time. */
        wall_render_state.transform = *(sfTransform*) &wall_trans_mat;
        sfRenderWindow_drawPrimitives(rwindow, wall_points, 6, sfTriangles,
                &wall_render_state);

        return;
}

void
render()
{
        sfRenderWindow_clear(rwindow, sfWhite);

        /**
         * Fix the non-translational code that keeps me away from a 8-variable
         * linear system.
        */
        sfView_setSize(view, (sfVector2f) { winsize.x, winsize.y });
        sfView_setCenter(view, (sfVector2f) { winsize.x / 2, 0 });
        sfRenderWindow_setView(rwindow, view);

        sfRenderWindow_drawRectangleShape(rwindow, floor_rect, NULL);

        /* Sort walls by proximity to cam. Only consistent w/ average cases. */
        qsort(walls, walls_n, sizeof (Wall), (__compar_fn_t) wall_compare);

        /* Render each wall, or at least try. */
        for (size_t i = 0; i < walls_n; i++) {

                /* Instead of moving the camera, move the world. */

                v2 va = { walls[i].a.x - cam.pos.x, walls[i].a.y - cam.pos.y };
                v2 vb = { walls[i].b.x - cam.pos.x, walls[i].b.y - cam.pos.y };

                double theta_a = v2_angle(cam.dir, va);
                double theta_b = v2_angle(cam.dir, vb);

                double a_dist = cam.dir.x * va.x + cam.dir.y * va.y;
                double b_dist = cam.dir.x * vb.x + cam.dir.y * vb.y;

                bool a_behind = a_dist < 0; /* same as 'fabs(theta_a) > PI/2.' */
                bool b_behind = b_dist < 0; /* same as 'fabs(theta_b) > PI/2.' */

                /* If the wall is behind the camera plane, skip all rendering */
                if (a_behind && b_behind)
                        continue;

                /* If the wall is outside the camera view... */
                if (fabs(theta_a) > cam.fov/2 && fabs(theta_b) > cam.fov/2)
                        /* ... and both on the same side, skip rendering. */
                        if (theta_a * theta_b > 0.)
                                continue;

                float padd = 1.f;

                /* Something **viewable** behind us? We need to fix. */
                if (a_behind || b_behind) {

                        v2 new;

                        /**
                         * Calculate the intersection.
                         * https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection#Given_two_points_on_each_line
                        */

                        double f = -(va.x * vb.y - va.y * vb.x) /
                                ( cam.dir.y * (va.y - vb.y) +
                                  cam.dir.x * (va.x - vb.x) );

                        new.x =  f * cam.dir.y;
                        new.y = -f * cam.dir.x;

                        /* Crap code. */

                        float mod = v2_distance(va, vb);

                        if (a_behind) {
                                padd = -v2_distance(new, vb) / mod;
                                va = new;
                                va.x = va.x + (vb.x - va.x) * 0.00001;
                                va.y = va.y + (vb.y - va.y) * 0.00001;
                                a_dist = (cam.dir.x * va.x + cam.dir.y * va.y);
                                theta_a = v2_angle(cam.dir, va);
                        }
                        else {
                                padd = v2_distance(new, va) / mod;
                                vb = new;
                                vb.x = vb.x + (va.x - vb.x) * 0.00001;
                                vb.y = vb.y + (va.y - vb.y) * 0.00001;
                                b_dist = (cam.dir.x * vb.x + cam.dir.y * vb.y);
                                theta_b = v2_angle(cam.dir, vb);
                        }

                }

                /**
                 * I spent 2 F** DAYS searching why the wall was incorrectly
                 * displayed. I knew it wasn't the fish eye artifact, as I got
                 * the "non-euclidean" distances by the point-to-rect formula,
                 * so the error probably was in the angle-to-screen part. After
                 * reviewing the this article https://lodev.org/cgtutor/raycasting.html,
                 * I found that the screen-to-angle formula was made of an atan
                 * function, so in my case, I only needed to make the inverse,
                 * tan, and get the appropiate correspondent x value from it.
                */

                /**
                 * Minimalistic version:
                 * double a_x = (tan(theta_a)/cam.fov + 0.5) * winsize_render.x;
                 * double b_x = (tan(theta_b)/cam.fov + 0.5) * winsize_render.x;
                */

                /**
                 * This is what we should really use. To understand why, u can
                 * visit: https://www.desmos.com/calculator/ubatkqgq3e
                */

                double fovtan = (2*tan(cam.fov/2.));
                double a_x = (tan(theta_a) / fovtan + 0.5) * winsize_render.x;
                double b_x = (tan(theta_b) / fovtan + 0.5) * winsize_render.x;

                //double winprop = winsize.x / (double) winsize.y;
                double a_h = 4 * winsize.y / a_dist;
                double b_h = 4 * winsize.y / b_dist;

                render_wall(a_x, a_h, b_x, b_h, padd);
        }

        sfRectangleShape_setPosition(cursor, (sfVector2f) { winsize.x / 2.f - 5, 0.f - 5});
        sfRenderWindow_drawRectangleShape(rwindow, cursor, NULL);

        sfRenderWindow_display(rwindow);
}

void
handle_events()
{
        sfEvent event;

        double mouse_delta = 0;

        while (sfRenderWindow_pollEvent(rwindow, &event)) {
                if (event.type == sfEvtClosed) {
                        sfRenderWindow_close(rwindow);
                }
                else if (event.type == sfEvtMouseMoved) {
                        /* ... */
                }
                else if (event.type == sfEvtKeyPressed) {
                        if (event.key.code == sfKeyLShift) {
                                cam.vel *= 3;
                        }
                }
                else if (event.type == sfEvtKeyReleased) {
                        if (event.key.code == sfKeyLShift) {
                                cam.vel /= 3;
                        }
                }
                else if (event.type == sfEvtResized) {
                        winsize = sfRenderWindow_getSize(rwindow);
                }
        }

        /* Mouse movement, nasty code, but SFML does not better. */
        if (sfRenderWindow_hasFocus(rwindow)) {

                mouse = sfMouse_getPositionRenderWindow(rwindow);
                mouse_delta = -(mouse.x - (int) winsize.x / 2);

                sfMouse_setPositionRenderWindow(
                        (sfVector2i) { winsize.x / 2, winsize.y / 2 }, rwindow
                );

                cam.dir = v2_rotate(cam.dir, mouse_delta * MOUSE_DELTA_FACTOR * dtime);
        }

        if (sfKeyboard_isKeyPressed(sfKeyW)) {
                cam.pos.x += cam.dir.x * cam.vel * dtime;
                cam.pos.y += cam.dir.y * cam.vel * dtime;
        }
        if (sfKeyboard_isKeyPressed(sfKeyS)) {
                cam.pos.x -= cam.dir.x * cam.vel * dtime;
                cam.pos.y -= cam.dir.y * cam.vel * dtime;
        }
        if (sfKeyboard_isKeyPressed(sfKeyA)) {
                cam.pos.x += cam.dir.y * cam.vel * dtime;
                cam.pos.y -= cam.dir.x * cam.vel * dtime;
        }
        if (sfKeyboard_isKeyPressed(sfKeyD)) {
                cam.pos.x -= cam.dir.y * cam.vel * dtime;
                cam.pos.y += cam.dir.x * cam.vel * dtime;
        }
 
        if (sfKeyboard_isKeyPressed(sfKeyAdd)) {
                cam.fov += 0.001 * dtime;
        }
        if (sfKeyboard_isKeyPressed(sfKeySubtract)) {
                cam.fov -= 0.001 * dtime;
        }

}

/* TODO: Add a "map editor" module */

void
wall_loader(const char* filename) {
        return;
}

int
main(int argc, char **argv)
{
        sfVideoMode video_mode = { winsize_render.x, winsize_render.y, 32 };
        rwindow = sfRenderWindow_create(video_mode, "doomb", sfClose, NULL);

        sfRenderWindow_setMouseCursorGrabbed(rwindow, true);
        sfRenderWindow_setMouseCursorVisible(rwindow, false);
        sfRenderWindow_setVerticalSyncEnabled(rwindow, false);

        winsize = sfRenderWindow_getSize(rwindow);

        if (!rwindow)
                return EXIT_FAILURE;

        wall_tex = sfTexture_createFromFile("assets/wall.png", NULL);

        if (!wall_tex)
                return EXIT_FAILURE;

        wall_render_state = sfRenderStates_default();
        wall_render_state.texture = wall_tex;
        wall_render_state.transform = *(sfTransform*) &wall_trans_mat;

        wall_tex_size = sfTexture_getSize(wall_tex);

        /* Resetting the unit square. More explanation in the render function. */
        wall_points[0].position = (sfVector2f) { 0, 0 };
        wall_points[1].position = (sfVector2f) { 0, 1 };
        wall_points[2].position = (sfVector2f) { 1, 0 };
        wall_points[3].position = (sfVector2f) { 0, 1 };
        wall_points[4].position = (sfVector2f) { 1, 0 };
        wall_points[5].position = (sfVector2f) { 1, 1 };

        wall_points[0].texCoords = (sfVector2f) { 0.f, 0.f };
        wall_points[1].texCoords = (sfVector2f) { 0.f, wall_tex_size.y };
        wall_points[2].texCoords = (sfVector2f) { wall_tex_size.x, 0.f };
        wall_points[3].texCoords = (sfVector2f) { 0.f, wall_tex_size.y };
        wall_points[4].texCoords = (sfVector2f) { wall_tex_size.x, 0.f };
        wall_points[5].texCoords = (sfVector2f) { wall_tex_size.x, wall_tex_size.y };

        wall_points[0].color = sfWhite;
        wall_points[1].color = sfWhite;
        wall_points[2].color = sfWhite;
        wall_points[3].color = sfWhite;
        wall_points[4].color = sfWhite;
        wall_points[5].color = sfWhite;

        floor_tex = sfTexture_createFromFile("assets/floor.png", NULL);

        if (!floor_tex)
                return EXIT_FAILURE;

        floor_rect = sfRectangleShape_create();
        sfRectangleShape_setTexture(floor_rect, floor_tex, true);
        sfRectangleShape_setSize(floor_rect, (sfVector2f) { winsize.x, winsize.y / 2.f });
        sfRectangleShape_setPosition(floor_rect, (sfVector2f) { 0, 0 });
        sfRectangleShape_setFillColor(floor_rect, sfWhite);

        view = sfView_create();
        sfView_setSize(view, (sfVector2f) { winsize.x, winsize.y });
        sfView_setViewport(view, (sfFloatRect) { 0, 0, 1, 1 });
        sfView_setCenter(view, (sfVector2f){ winsize.x / 2.f,0 });
        sfRenderWindow_setView(rwindow, view);

        cursor = sfRectangleShape_create();
        sfRectangleShape_setSize(cursor, (sfVector2f) { 10.f, 10.f });
        sfRectangleShape_setPosition(cursor, (sfVector2f) { winsize.x / 2.f - 5, 0.f + 5 });
        sfRectangleShape_setFillColor(cursor, sfRed);

        walls_n = 8;
        walls = malloc(walls_n * sizeof (Wall));

        /* First map column */

        walls[0].b.x = 10.;
        walls[0].b.y = 10.;
        walls[0].a.x = 10.;
        walls[0].a.y = 20.;

        walls[1].b.x = 10.;
        walls[1].b.y = 10.;
        walls[1].a.x = 20.;
        walls[1].a.y = 10.;

        walls[2].b.x = 20.;
        walls[2].b.y = 20.;
        walls[2].a.x = 20.;
        walls[2].a.y = 10.;

        walls[3].b.x = 20.;
        walls[3].b.y = 20.;
        walls[3].a.x = 10.;
        walls[3].a.y = 20.;

        /* Second map column */

        walls[4].b.x = 10. + 20.f;
        walls[4].b.y = 10.;
        walls[4].a.x = 10. + 20.f;
        walls[4].a.y = 20.;

        walls[5].b.x = 10. + 20.f;
        walls[5].b.y = 10.;
        walls[5].a.x = 20. + 20.f;
        walls[5].a.y = 10.;

        walls[6].b.x = 20. + 20.f;
        walls[6].b.y = 20.;
        walls[6].a.x = 20. + 20.f;
        walls[6].a.y = 10.;

        walls[7].b.x = 20. + 20.f;
        walls[7].b.y = 20.;
        walls[7].a.x = 10. + 20.f;
        walls[7].a.y = 20.;

        cam.pos = (v2) {0., 0.};
        cam.dir = (v2) {1., 0.};
        cam.vel = 0.04;
        cam.fov = 100 * (PI / 180);

        cam.dir = v2_rotate(cam.dir, -45 * (PI/180));

        clock = sfClock_create();

        while (sfRenderWindow_isOpen(rwindow)) {

                dtime = sfClock_restart(clock).microseconds / 1000.;

                handle_events();

                render();
        }

        free(walls);

        sfView_destroy(view);
        sfTexture_destroy(wall_tex);
        sfRectangleShape_destroy(cursor);
        sfRectangleShape_destroy(floor_rect);
        sfTexture_destroy(floor_tex);
        sfClock_destroy(clock);
        sfRenderWindow_destroy(rwindow);

        return EXIT_SUCCESS;
}
