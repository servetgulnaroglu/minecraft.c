// Based on a 3D raycasting example I studied, modified for terminal input and color output.

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <termios.h>
#include <stdlib.h>
#include <math.h>
#define Y_PIXELS 180
#define X_PIXELS 900
#define Z_BLOCKS 10
#define Y_BLOCKS 20
#define EYE_HEIGHT 1.5
#define X_BLOCKS 20
#define VIEW_HEIGHT 0.7
#define VIEW_WIDTH 1
#define BLOCK_BORDER_SIZE 0.05


static struct termios old_termios, new_termios;

typedef struct Vector {
    float x;
    float y;
    float z;
} vect;
typedef struct Vector2 {
    float psi;
    float phi;
} vect2;


typedef struct Vector_vector2 {
    vect pos;
    vect2 view;
} player_pos_view;

void init_terminal() {
    tcgetattr(STDIN_FILENO, &old_termios);
    new_termios = old_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
    fflush(stdout);
}

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    printf("terminal restored");
}

static char keystate[256] = { 0 };

void process_input() {
    char c;
    for (int i = 0; i < 256; i++) {
        keystate[i] = 0;
    }

    while (read(STDIN_FILENO, &c, 1) > 0) {
        // printf("\ninput: %c", c);
        unsigned char uc = (unsigned char)c;
        keystate[uc] = 1;
    }
}

int is_key_pressed(char key) {
    return keystate[(unsigned char)key];
}

char** init_picture() {
    char** picture = malloc(sizeof(char*) * Y_PIXELS);
    for (int i = 0; i < Y_PIXELS; i++) {
        picture[i] = malloc(sizeof(char) * X_PIXELS);
    }
    return picture;
}

void free_picture(char** picture) {
    if (!picture) return;
    
    for (int i = 0; i < Y_PIXELS; i++) {
        free(picture[i]);
    }
    free(picture);
}

char*** init_blocks() {
    char*** blocks = malloc(sizeof(char**) * Z_BLOCKS);
    for (int i = 0; i < Z_BLOCKS; i++) {
        blocks[i] = malloc(sizeof(char*) * Y_BLOCKS);
        for (int j = 0; j < Y_BLOCKS; j++) {
            blocks[i][j] = malloc(sizeof(char) * X_BLOCKS);
            for (int k = 0; k < X_BLOCKS; k++) {
                blocks[i][j][k] = ' ';
            }
        }
    }
    return blocks;
}

void free_blocks(char*** blocks) {
    if (!blocks) return;

    for (int i = 0; i < Z_BLOCKS; i++) {
        if (!blocks[i]) continue;
        for (int j = 0; j < Y_BLOCKS; j++) {
            free(blocks[i][j]);
        }
        free(blocks[i]);
    }
    free(blocks);
}

player_pos_view init_posview() {
    player_pos_view posview;
    posview.pos.x = 5;
    posview.pos.y = 5;
    posview.pos.z = 4 + EYE_HEIGHT;
    posview.view.phi = 0;
    posview.view.psi = 0;
    return posview;
}
vect angles_to_vect(vect2 angles) {
    vect res;
    res.x = cos(angles.psi) * cos(angles.phi);
    res.y = cos(angles.psi) * sin(angles.phi);
    res.z = sin(angles.psi);
    return res;
}
vect vect_add(vect v1, vect v2) {
    vect res;
    res.x = v1.x + v2.x;
    res.y = v1.y + v2.y;
    res.z = v1.z + v2.z;
    return res;
}

vect vect_scale(float s, vect v) {
    vect res = { s * v.x, s * v.y, s * v.z };
    return res;
}

vect vect_sub(vect v1, vect v2) {
    vect v3 = vect_scale(-1, v2);
    return vect_add(v1, v3);
}
void vect_normalize(vect* v) {
    float len = sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
    v->x /= len;
    v->y /= len;
    v->z /= len;
}

vect** init_directions(vect2 view) {
    view.psi -= VIEW_HEIGHT / 2.0;
    vect screen_down = angles_to_vect(view);
    view.psi += VIEW_HEIGHT;
    vect screen_up = angles_to_vect(view);
    view.psi -= VIEW_HEIGHT / 2.0;
    view.phi -= VIEW_WIDTH / 2.0;
    vect screen_left = angles_to_vect(view);
    view.phi += VIEW_WIDTH;
    vect screen_right = angles_to_vect(view);
    view.phi -= VIEW_WIDTH / 2.0;

    vect screen_mid_vert = vect_scale(0.5, vect_add(screen_up, screen_down));
    vect screen_mid_hor = vect_scale(0.5, vect_add(screen_left, screen_right));
    vect mid_to_left = vect_sub(screen_left, screen_mid_hor);
    vect mid_to_up = vect_sub(screen_up, screen_mid_vert);

    vect** dir = malloc(sizeof(vect*) * Y_PIXELS);
    for (int i = 0; i < Y_PIXELS; i++) {
        dir[i] = malloc(sizeof(vect) * X_PIXELS);
    }
    for (int y_pix = 0; y_pix < Y_PIXELS; y_pix++) {
        for (int x_pix = 0; x_pix < X_PIXELS; x_pix++) {
            vect tmp = vect_add(vect_add(screen_mid_hor, mid_to_left), mid_to_up);
            tmp = vect_sub(tmp, vect_scale(((float)x_pix / (X_PIXELS - 1)) * 2, mid_to_left));
            tmp = vect_sub(tmp, vect_scale(((float)y_pix / (Y_PIXELS - 1)) * 2, mid_to_up));
            vect_normalize(&tmp);
            dir[y_pix][x_pix] = tmp;
        }
    }
    return dir;
}

void free_directions(vect** directions) {
    if (!directions) return;
    
    for (int y = 0; y < Y_PIXELS; y++) {
        free(directions[y]);
    }
    free(directions);
}

int ray_outside(vect pos) {
    if (pos.x >= X_BLOCKS || pos.y >= Y_BLOCKS || pos.z >= Z_BLOCKS
        || pos.x < 0 || pos.y < 0 || pos.z < 0) {
        return 1;
    }
    return 0;
}

int on_block_border(vect pos) {
    int cnt = 0;
    if (fabsf(pos.x - roundf(pos.x)) < BLOCK_BORDER_SIZE) {
        cnt++;
    }
    if (fabsf(pos.y - roundf(pos.y)) < BLOCK_BORDER_SIZE) {
        cnt++;
    }
    if (fabsf(pos.z - roundf(pos.z)) < BLOCK_BORDER_SIZE) {
        cnt++;
    }
    if (cnt >= 2) {
        return 1;
    }
    return 0;
}

float min(float a, float b) {
    if (a < b)
        return a;
    return b;
}

char raytrace(vect pos, vect dir, char*** blocks) {
    float eps = 0.01;
    while (!ray_outside(pos)) {
        char c = blocks[(int)pos.z][(int)pos.y][(int)pos.x];
        if (c != ' ') {
            if (on_block_border(pos)) {
                return '-';
            }
            else {
                return c;
            }
        }
        float dist = 2;
        if (dir.x > eps) {
            dist = min(dist, ((int)(pos.x + 1) - pos.x) / dir.x);
        }
        else if (dir.x < -eps) {
            dist = min(dist, ((int)pos.x - pos.x) / dir.x);
        }
        if (dir.y > eps) {
            dist = min(dist, ((int)(pos.y + 1) - pos.y) / dir.y);
        }
        else if (dir.y < -eps) {
            dist = min(dist, ((int)pos.y - pos.y) / dir.y);
        }
        if (dir.z > eps) {
            dist = min(dist, ((int)(pos.z + 1) - pos.z) / dir.z);
        }
        else if (dir.z < -eps) {
            dist = min(dist, ((int)pos.z - pos.z) / dir.z);
        }
        pos = vect_add(pos, vect_scale(dist + eps, dir));
    }
    return ' ';
}

void get_picture(char** picture, player_pos_view posview, char*** blocks) {
    vect** directions = init_directions(posview.view);
    for (int y = 0; y < Y_PIXELS; y++) {
        for (int x = 0; x < X_PIXELS; x++) {
            picture[y][x] = raytrace(posview.pos, directions[y][x], blocks);
        }
    }
    free_directions(directions);
}

void draw_ascii(char** picture) {
    fflush(stdout);
    printf("\033[0;0H");
    for (int i = 0; i < Y_PIXELS; i++) {
        int current_color = 0;
        for (int j = 0; j < X_PIXELS; j++) {
            // printf("%c", picture[i][j]);
            if (picture[i][j] == 'o' && current_color != 32) {
                printf("\x1B[32m");
                current_color = 32;
            }
            else if (picture[i][j] != 'o' && current_color != 0) {
                printf("\x1B[0m");
                current_color = 0;
            }
            printf("%c", picture[i][j]);
        }
        printf("\x1B[0m\n");
    }
}

void update_pos_view(player_pos_view* posview, char*** blocks) {
    float move_eps = 0.30;
    float tilt_eps = 0.1;
    int x = (int)posview->pos.x, y = (int)posview->pos.y;
    int z = (int)posview->pos.z - EYE_HEIGHT + 0.01;
    if (blocks[z][y][x] != ' ') {
        posview->pos.z += 1;
    }
    z = (int)posview->pos.z - EYE_HEIGHT - 0.01;
    if (blocks[z][y][x] == ' ') {
        posview->pos.z -= 1;
    }


    if (is_key_pressed('w')) {
        posview->view.psi += tilt_eps;
    }
    if (is_key_pressed('s')) {
        posview->view.psi -= tilt_eps;
    }
    if (is_key_pressed('d')) {
        posview->view.phi += tilt_eps;
    }
    if (is_key_pressed('a')) {
        posview->view.phi -= tilt_eps;
    }
    vect direction = angles_to_vect(posview->view);
    if (is_key_pressed('i')) {
        posview->pos.x += move_eps * direction.x;
        posview->pos.y += move_eps * direction.y;
    }
    if (is_key_pressed('k')) {
        posview->pos.x -= move_eps * direction.x;
        posview->pos.y -= move_eps * direction.y;
    }
    if (is_key_pressed('j')) {
        posview->pos.x += move_eps * direction.y;
        posview->pos.y -= move_eps * direction.x;
    }
    if (is_key_pressed('l')) {
        posview->pos.x -= move_eps * direction.y;
        posview->pos.y += move_eps * direction.x;
    }
}

vect get_current_block(player_pos_view posview, char*** blocks) {
    vect pos = posview.pos;
    vect dir = angles_to_vect(posview.view);
    float eps = 0.01;
    while (!ray_outside(pos)) {
        char c = blocks[(int)pos.z][(int)pos.y][(int)pos.x];
        if (c != ' ') {
            return pos;
        }
        float dist = 2;
        if (dir.x > eps) {
            dist = min(dist, ((int)(pos.x + 1) - pos.x) / dir.x);
        }
        else if (dir.x < -eps) {
            dist = min(dist, ((int)pos.x - pos.x) / dir.x);
        }
        if (dir.y > eps) {
            dist = min(dist, ((int)(pos.y + 1) - pos.y) / dir.y);
        }
        else if (dir.y < -eps) {
            dist = min(dist, ((int)pos.y - pos.y) / dir.y);
        }
        if (dir.z > eps) {
            dist = min(dist, ((int)(pos.z + 1) - pos.z) / dir.z);
        }
        else if (dir.z < -eps) {
            dist = min(dist, ((int)pos.z - pos.z) / dir.z);
        }
        pos = vect_add(pos, vect_scale(dist + eps, dir));
    }
    return pos;
}

void place_block(vect pos, char*** blocks, char block) {
    int x = (int)pos.x, y = (int)pos.y, z = (int)pos.z;
    float dists[6];
    dists[0] = fabsf(x + 1 - pos.x);
    dists[1] = fabsf(pos.x - x);
    dists[2] = fabsf(y + 1 - pos.y);
    dists[3] = fabsf(pos.y - y);
    dists[4] = fabsf(z + 1 - pos.z);
    dists[5] = fabsf(pos.z - z);
    int min = 0;
    float mindist = dists[0];
    for (int i = 0; i < 6;i++) {
        if (dists[i] < mindist) {
            mindist = dists[i];
            min = i;
        }
    }
    switch (min) {
    case 0:
        blocks[z][y][x + 1] = block;
        break;
    case 1:
        blocks[z][y][x - 1] = block;
        break;
    case 2:
        blocks[z][y + 1][x] = block;
        break;
    case 3:
        blocks[z][y - 1][x] = block;
        break;
    case 4:
        blocks[z + 1][y][x] = block;
        break;
    case 5:
        blocks[z - 1][y][x] = block;
        break;
    default:
        break;
    }
}

int main() {
    init_terminal();
    char** picture = init_picture();
    char*** blocks = init_blocks();
    for (int x = 0; x < X_BLOCKS; x++) {
        for (int y = 0; y < Y_BLOCKS; y++) {
            for (int z = 0; z < 4; z++) {
                blocks[z][y][x] = '@';
            }
        }
    }
    player_pos_view posview = init_posview();
    while (1) {
        process_input();
        if (is_key_pressed('q')) {
            free_blocks(blocks);
            free_picture(picture);
            restore_terminal();
            exit(0);
        }
        update_pos_view(&posview, blocks);
        vect current_block = get_current_block(posview, blocks);
        int have_current_block = !ray_outside(current_block);
        int current_block_x = current_block.x;
        int current_block_y = current_block.y;
        int current_block_z = current_block.z;
        char current_block_c;
        int removed = 0;
        if (have_current_block) {
            current_block_c = blocks[current_block_z][current_block_y][current_block_x];
            blocks[current_block_z][current_block_y][current_block_x] = 'o';
            if (is_key_pressed('x')) {
                removed = 1;
                blocks[current_block_z][current_block_y][current_block_x] = ' ';
            }

            if (is_key_pressed(' ')) {
                place_block(current_block, blocks, '@');
            }
        }

        get_picture(picture, posview, blocks);
        if (have_current_block && !removed) {
            blocks[current_block_z][current_block_y][current_block_x] = current_block_c;
        }
        draw_ascii(picture);
        usleep(20000);
    }
    restore_terminal();
    free_blocks(blocks);
    free_picture(picture);
    return 0;
}
