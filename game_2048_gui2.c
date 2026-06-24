/*
 * game_2048_gui.c  —  Game 2048 voi SDL2 + che do tu dong chay test case
 *
 * BIEN DICH:
 *   Linux : gcc game_2048_gui.c -o game -lSDL2 -lSDL2_ttf -lm
 *   Windows: gcc game_2048_gui.c -o game.exe -lSDL2 -lSDL2_ttf -lm
 *
 * CHAY:
 *   Che do tay    : ./game
 *   Che do tu dong: ./game ALL_TESTCASES.txt
 *                   ./game tc01_Di_chuyen_trai__khong_gop.txt
 *
 * DINH DANG FILE TEST (.txt):
 *   # comment
 *   BEGIN_TEST TC-01        <- bat dau test case moi, reset board ve 0
 *   INIT r c v              <- dat gia tri v vao o [r][c]  (0 <= r,c <= 3)
 *   a / w / s / d           <- lenh di chuyen
 *   n                       <- new game
 *   END_TEST                <- in ket qua ra terminal, chuyen test tiep theo
 *   q                       <- thoat chuong trinh
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>

/* =========================================================
   HANG SO
   ========================================================= */
#define WINDOW_WIDTH   620
#define WINDOW_HEIGHT  720
#define bang_SIZE      4
#define CELL_SIZE      110
#define CELL_MARGIN    15
#define BOARD_OFFSET_X ((WINDOW_WIDTH - (CELL_SIZE * bang_SIZE + CELL_MARGIN * (bang_SIZE + 1))) / 2)
#define BOARD_OFFSET_Y 180

/* Toc do tu dong: ms giua moi lenh (tang len neu muon xem ro hon) */
#define AUTO_STEP_DELAY_MS 700

/* =========================================================
   MAU SAC
   ========================================================= */
typedef struct { Uint8 r, g, b; } Color;

Color COLORS[] = {
    {205, 193, 180},  /* 0     — o trong    */
    {238, 228, 218},  /* 2                  */
    {237, 224, 200},  /* 4                  */
    {242, 177, 121},  /* 8                  */
    {245, 149,  99},  /* 16                 */
    {246, 124,  95},  /* 32                 */
    {246,  94,  59},  /* 64                 */
    {237, 207, 114},  /* 128                */
    {237, 204,  97},  /* 256                */
    {237, 200,  80},  /* 512                */
    {237, 197,  63},  /* 1024               */
    {237, 194,  46},  /* 2048               */
    { 60,  58,  50},  /* > 2048             */
};
Color TEXT_LIGHT = {249, 246, 242};
Color TEXT_DARK  = {119, 110, 101};

Color get_tile_color(int value) {
    if (value == 0) return COLORS[0];
    int index = (int)log2(value);
    if (index > 12) index = 12;
    return COLORS[index];
}
Color get_text_color(int value) {
    if (value <= 4) return TEXT_DARK;
    return TEXT_LIGHT;
}

/* =========================================================
   CAU TRUC GAME
   ========================================================= */
typedef struct game {
    int    bang[bang_SIZE][bang_SIZE];
    int    current_score;
    int    best_score;
    SDL_Window*   window;
    SDL_Renderer* renderer;
    TTF_Font*     font_title;
    TTF_Font*     font_large;
    TTF_Font*     font_medium;
    TTF_Font*     font_small;
    char   status_msg[256];
    Uint32 msg_time;

    /* --- Che do tu dong --- */
    FILE*  input_file;       /* NULL = chay tay */
    int    auto_delay_ms;    /* ms giua moi buoc */
    Uint32 next_auto_tick;   /* thoi diem xu ly lenh tiep theo */
    int    test_index;       /* so thu tu test case hien tai    */
    char   cur_test_id[64];  /* ten test case hien tai          */
    int    auto_done;
} Game;

/* =========================================================
   TIEN KHAI BAO
   ========================================================= */
void draw_header(Game* game);
void draw_board(Game* game);
void render(Game* game);
void cleanup(Game* game);

/* =========================================================
   KHOI TAO SDL
   ========================================================= */
int init_sdl(Game* game) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 0;
    }
    if (TTF_Init() == -1) {
        printf("TTF_Init Error: %s\n", TTF_GetError());
        return 0;
    }
    game->window = SDL_CreateWindow(
        "2048  [Auto Test Mode — xem terminal]",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!game->window) return 0;

    game->renderer = SDL_CreateRenderer(game->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!game->renderer) return 0;

    strcpy(game->status_msg, "");
    game->msg_time = 0;
    return 1;
}

/* =========================================================
   LOAD FONT
   ========================================================= */
int load_fonts(Game* game) {
    const char* fonts[] = {
        "C:\\Windows\\Fonts\\arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
        NULL
    };
    for (int i = 0; fonts[i]; i++) {
        game->font_title = TTF_OpenFont(fonts[i], 68);
        if (game->font_title) {
            game->font_large  = TTF_OpenFont(fonts[i], 48);
            game->font_medium = TTF_OpenFont(fonts[i], 32);
            game->font_small  = TTF_OpenFont(fonts[i], 20);
            return 1;
        }
    }
    printf("Khong load duoc font. Chu se khong hien.\n");
    return 0;
}

/* =========================================================
   LOGIC GAME
   ========================================================= */
int dichHang[] = { 1,  0, -1,  0};
int dichCot[]  = { 0,  1,  0, -1};

typedef struct { int hang, cot; } viTriO;

/* Kiem tra xem bang con o trong khong */
int co_o_trong(Game* game) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (game->bang[i][j] == 0) return 1;
    return 0;
}

/* Tim o trong ngau nhien — da sua bug infinite loop */
viTriO checkOTrong(Game* game) {
    viTriO res = {-1, -1};
    if (!co_o_trong(game)) return res;   /* tranh loop vo tan khi bang day */
    int hang, cot;
    do {
        hang = rand() % 4;
        cot  = rand() % 4;
    } while (game->bang[hang][cot] != 0);
    res.hang = hang;
    res.cot  = cot;
    return res;
}

void themSo(Game* game) {
    viTriO v = checkOTrong(game);
    if (v.hang == -1) return;            /* bang day, bo qua */
    game->bang[v.hang][v.cot] = 2;
}

void newGame(Game* game) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            game->bang[i][j] = 0;
    game->current_score = 0;
    game->msg_time = 0;
    strcpy(game->status_msg, "");
    themSo(game);
    themSo(game);
}

bool kiemTraLenh(Game* game, int hang, int cot, int ni, int nj) {
    if (ni < 0 || ni > 3 || nj < 0 || nj > 3) return false;
    if (game->bang[hang][cot] != game->bang[ni][nj] && game->bang[ni][nj] != 0) return false;
    return true;
}

void thucThiLenh(Game* game, int huong) {
    int hangDau = 0, cotDau = 0, buocH = 1, buocC = 1;
    if (huong == 0) { hangDau = 3; buocH = -1; }
    if (huong == 1) { cotDau  = 3; buocC = -1; }

    int da_gop[4][4] = {0};  /* danh dau o nao da duoc gop trong luot nay */
    int changed, coMove = 0;

    do {
        changed = 0;
        for (int i = hangDau; i >= 0 && i < 4; i += buocH) {
            for (int j = cotDau; j >= 0 && j < 4; j += buocC) {
                int ni = i + dichHang[huong];
                int nj = j + dichCot[huong];

                if (game->bang[i][j] == 0) continue;
                if (!kiemTraLenh(game, i, j, ni, nj)) continue;

                if (game->bang[ni][nj] != 0) {
                    /* Gop: chi duoc gop neu ca hai o chua duoc gop lan nao */
                    if (da_gop[i][j] || da_gop[ni][nj]) continue;
                    game->current_score += game->bang[i][j] * 2;
                    if (game->current_score > game->best_score)
                        game->best_score = game->current_score;
                    game->bang[ni][nj] += game->bang[i][j];
                    game->bang[i][j]    = 0;
                    da_gop[ni][nj] = 1;  /* danh dau o dich da gop */
                } else {
                    /* Truot vao o trong: luon cho phep */
                    game->bang[ni][nj] = game->bang[i][j];
                    game->bang[i][j]   = 0;
                    /* Ke thua trang thai gop cua o cu sang o moi */
                    da_gop[ni][nj] = da_gop[i][j];
                    da_gop[i][j]   = 0;
                }
                changed = 1;
                coMove  = 1;
            }
        }
    } while (changed);

    if (coMove) themSo(game);
}

int checkEnd(Game* game) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (game->bang[i][j] == 0) return 0;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            if (j < 3 && game->bang[i][j] == game->bang[i][j+1]) return 0;
            if (i < 3 && game->bang[i][j] == game->bang[i+1][j]) return 0;
        }
    return 1;
}
int checkWin(Game* game) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (game->bang[i][j] == 2048) return 1;
    return 0;
}
/* =========================================================
   IN BANG RA TERMINAL (cho che do tu dong)
   ========================================================= */
void print_board(Game* game) {
    printf("  +------+------+------+------+\n");
    for (int i = 0; i < 4; i++) {
        printf("  |");
        for (int j = 0; j < 4; j++) {
            if (game->bang[i][j])
                printf("%5d |", game->bang[i][j]);
            else
                printf("      |");
        }
        printf("\n  +------+------+------+------+\n");
    }
    printf("  Score: %d   Best: %d\n", game->current_score, game->best_score);
}

/* =========================================================
   DOC LENH TU FILE
   
   Tra ve:
     SDLK_w / s / a / d  — lenh di chuyen
     SDLK_n              — new game
     SDLK_q              — thoat
     -2                  — da xu ly INIT / BEGIN / comment (doc tiep)
     -1                  — het file
   ========================================================= */
int doc_lenh_tiep_theo(Game* game) {
    if (!game->input_file) return -1;

    char line[256];
    while (fgets(line, sizeof(line), game->input_file)) {
        /* Xoa newline */
        line[strcspn(line, "\r\n")] = 0;

        /* Bo qua dong trong va comment */
        if (line[0] == '\0' || line[0] == '#') continue;

        /* ---- INIT hang cot gia_tri ---- */
        if (strncmp(line, "INIT", 4) == 0) {
            int r, c, v;
            if (sscanf(line + 4, "%d %d %d", &r, &c, &v) == 3
                && r >= 0 && r < 4 && c >= 0 && c < 4)
            {
                game->bang[r][c] = v;
                printf("[AUTO]   INIT bang[%d][%d] = %d\n", r, c, v);
            }
            return -2; 
        }

        /* ---- BEGIN_TEST <id> ---- */
        if (strncmp(line, "BEGIN_TEST", 10) == 0) {
            game->test_index++;
            /* Lay ten test id (phan sau "BEGIN_TEST ") */
            const char* id_start = line + 10;
            while (*id_start == ' ') id_start++;
            strncpy(game->cur_test_id, id_start, sizeof(game->cur_test_id) - 1);
            game->cur_test_id[sizeof(game->cur_test_id) - 1] = '\0';

            /* Reset board va score cho test moi */
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                    game->bang[i][j] = 0;
            game->current_score = 0;
            strcpy(game->status_msg, "");
            game->msg_time = 0;

            printf("\n======================================================\n");
            printf("[AUTO] Test #%d: %s\n", game->test_index, game->cur_test_id);
            printf("======================================================\n");
            printf("[AUTO] Board khoi tao:\n");
            print_board(game);
            return -2;
        }

        /* ---- END_TEST ---- */
        if (strncmp(line, "END_TEST", 8) == 0) {
            printf("\n[AUTO] Ket qua %s:\n", game->cur_test_id);
            print_board(game);
            if (checkWin(game))
                printf("[AUTO] >> YOU WIN! 2048 reached!\n");
            else if (checkEnd(game))
                printf("[AUTO] >> GAME OVER\n");
            printf("------------------------------------------------------\n");

            /* Cap nhat tieu de cua so */
            char title[128];
            snprintf(title, sizeof(title), "2048  |  Test #%d: %s  |  Score: %d",
                     game->test_index, game->cur_test_id, game->current_score);
            SDL_SetWindowTitle(game->window, title);

            /* Hien thi ket qua tren man hinh mot chut truoc khi chuyen */
            snprintf(game->status_msg, sizeof(game->status_msg),
                     "Xong %s — Score: %d", game->cur_test_id, game->current_score);
            game->msg_time = SDL_GetTicks() + 2000;
            return -2;
        }

        /* ---- Phim di chuyen ---- */
        char ch = (char)tolower((unsigned char)line[0]);
        if (ch == 'w') { printf("[AUTO] [%s] Lenh: W (len)\n",   game->cur_test_id); return SDLK_w; }
        if (ch == 's') { printf("[AUTO] [%s] Lenh: S (xuong)\n", game->cur_test_id); return SDLK_s; }
        if (ch == 'a') { printf("[AUTO] [%s] Lenh: A (trai)\n",  game->cur_test_id); return SDLK_a; }
        if (ch == 'd') { printf("[AUTO] [%s] Lenh: D (phai)\n",  game->cur_test_id); return SDLK_d; }
        if (ch == 'n') { printf("[AUTO] [%s] Lenh: N (moi)\n",   game->cur_test_id); return SDLK_n; }
        if (ch == 'q') { printf("[AUTO] Lenh: Q (thoat)\n");                         return SDLK_q; }

        printf("[AUTO] Dong khong nhan dang duoc: '%s'\n", line);
    }

    /* Het file */
    printf("\n[AUTO] Da chay xong %d test case(s).\n", game->test_index);
    printf("[AUTO] Nhan Q hoac dong cua so de thoat.\n");
    fclose(game->input_file);
    game->input_file = NULL;
    game->auto_done  = 1;
    SDL_SetWindowTitle(game->window, "2048  |  Hoan thanh test — Nhan Q de thoat");
    snprintf(game->status_msg, sizeof(game->status_msg),
        "Hoan thanh %d test case(s) — Nhan Q de thoat", game->test_index);
    game->msg_time = SDL_GetTicks() + 999999;
    return -1;   /* khong tra SDLK_q, giu man hinh lai */
}

/* =========================================================
   XU LY PHIM (dung chung cho ca 2 che do)
   ========================================================= */
void xu_ly_phim(Game* game, SDL_Keycode sym, int* running) {
    switch (sym) {
        case SDLK_q: case SDLK_ESCAPE:
            *running = 0;
            break;
        case SDLK_n:
            newGame(game);
            break;
        case SDLK_UP:    case SDLK_w:
            thucThiLenh(game, 2);
            break;
        case SDLK_DOWN:  case SDLK_s:
            thucThiLenh(game, 0);
            break;
        case SDLK_LEFT:  case SDLK_a:
            thucThiLenh(game, 3);
            break;
        case SDLK_RIGHT: case SDLK_d:
            thucThiLenh(game, 1);
            break;
        default:
            break;
    }
    if (checkWin(game) == 1) {
        strcpy(game->status_msg, "YOU WIN! 2048! Nhan 'N' de choi lai");
        game->msg_time = SDL_GetTicks() + 100000;
    } 
    else if (checkEnd(game) == 1) {
        strcpy(game->status_msg, "GAME OVER! Nhan 'N' de choi lai");
        game->msg_time = SDL_GetTicks() + 100000;
    }
}

/* =========================================================
   MAIN
   ========================================================= */
int main(int argc, char* argv[]) {
    Game game = {0};

    if (!init_sdl(&game)) return 1;
    load_fonts(&game);
    srand((unsigned int)time(NULL));

    /* --- Kiem tra tham so dong lenh --- */
    if (argc >= 2) {
        game.input_file = fopen(argv[1], "r");
        if (game.input_file) {
            printf("[AUTO] ====================================\n");
            printf("[AUTO] Chay tu dong tu file: %s\n", argv[1]);
            printf("[AUTO] Toc do: %d ms/lenh\n", AUTO_STEP_DELAY_MS);
            printf("[AUTO] ====================================\n");
            game.auto_delay_ms = AUTO_STEP_DELAY_MS;

            /* Che do tu dong: bat dau voi bang trong */
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                    game.bang[i][j] = 0;
            strcpy(game.status_msg, "Che do tu dong — xem terminal de biet ket qua");
            game.msg_time = SDL_GetTicks() + 3000;
        } else {
            printf("[AUTO] Khong mo duoc file '%s'. Chay binh thuong.\n", argv[1]);
            newGame(&game);
        }
    } else {
        /* Che do tay binh thuong */
        newGame(&game);
    }

    game.next_auto_tick = SDL_GetTicks() + 500; /* cho 0.5s truoc lenh dau */

    int running = 1;
    SDL_Event event;

    while (running) {
        /* --- Xu ly event nguoi dung --- */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                /* Cho phep nguoi dung can thiep ngay ca khi chay file */
                xu_ly_phim(&game, event.key.keysym.sym, &running);
            }
        }

        /* --- Che do tu dong: doc lenh tu file --- */
        if (game.input_file && !game.auto_done && SDL_GetTicks() >= game.next_auto_tick) {
            int key = doc_lenh_tiep_theo(&game);           // chỉ đọc 1 dòng

            if (key == -2) {
                /* Dong dieu phoi (INIT/BEGIN/END/comment): render luon de thay board */
                render(&game);
                game.next_auto_tick = SDL_GetTicks() + 120; // delay ngan giua cac INIT
            } else {
                /* Lenh thuc su (wasd) hoac het file (-1) */
                if (key != -1 && key != SDLK_q)
                    xu_ly_phim(&game, (SDL_Keycode)key, &running);
                render(&game);
                game.next_auto_tick = SDL_GetTicks() + game.auto_delay_ms; // delay dai giua cac buoc
            }
        }

        render(&game);
        SDL_Delay(16);
    }

    if (game.input_file) fclose(game.input_file);
    cleanup(&game);
    return 0;
}

/* =========================================================
   VE GIAO DIEN
   ========================================================= */
void draw_text_centered(Game* game, const char* text,
                        int x, int y, int w, int h,
                        TTF_Font* font, Color c)
{
    if (!font || !text || strlen(text) == 0) return;
    SDL_Color color = {c.r, c.g, c.b, 255};
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(game->renderer, surface);
    SDL_Rect rect = {
        x + (w - surface->w) / 2,
        y + (h - surface->h) / 2,
        surface->w, surface->h
    };
    SDL_RenderCopy(game->renderer, texture, NULL, &rect);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void draw_header(Game* game) {
    draw_text_centered(game, "2048", 20, 20, 200, 100, game->font_title, COLORS[3]);

    char score_str[32];
    sprintf(score_str, "Score: %d", game->current_score);
    draw_text_centered(game, score_str, 400, 20, 150, 50, game->font_medium, COLORS[12]);

    char best_str[32];
    sprintf(best_str, "Best: %d", game->best_score);
    draw_text_centered(game, best_str, 400, 80, 150, 50, game->font_small, COLORS[12]);

    /* Hien thi mode hien tai */
    if (game->input_file) {
        char mode_str[128];
        snprintf(mode_str, sizeof(mode_str), "[AUTO] Test #%d: %s",
                 game->test_index, game->cur_test_id[0] ? game->cur_test_id : "...");
        draw_text_centered(game, mode_str, 0, 140, WINDOW_WIDTH, 30,
                           game->font_small, TEXT_DARK);
    }
}

void draw_board(Game* game) {
    SDL_Rect board_rect = {
        BOARD_OFFSET_X, BOARD_OFFSET_Y,
        bang_SIZE * CELL_SIZE + (bang_SIZE + 1) * CELL_MARGIN,
        bang_SIZE * CELL_SIZE + (bang_SIZE + 1) * CELL_MARGIN
    };
    SDL_SetRenderDrawColor(game->renderer, 187, 173, 160, 255);
    SDL_RenderFillRect(game->renderer, &board_rect);

    for (int i = 0; i < bang_SIZE; i++) {
        for (int j = 0; j < bang_SIZE; j++) {
            int val = game->bang[i][j];

            SDL_Rect cell = {
                BOARD_OFFSET_X + CELL_MARGIN + j * (CELL_SIZE + CELL_MARGIN),
                BOARD_OFFSET_Y + CELL_MARGIN + i * (CELL_SIZE + CELL_MARGIN),
                CELL_SIZE, CELL_SIZE
            };

            Color bg = get_tile_color(val);
            SDL_SetRenderDrawColor(game->renderer, bg.r, bg.g, bg.b, 255);
            SDL_RenderFillRect(game->renderer, &cell);

            if (val > 0) {
                char val_str[16];
                sprintf(val_str, "%d", val);
                Color txt = get_text_color(val);
                TTF_Font* fnt = (val >= 1024) ? game->font_medium : game->font_large;
                draw_text_centered(game, val_str, cell.x, cell.y, cell.w, cell.h, fnt, txt);
            }
        }
    }
}

void render(Game* game) {
    SDL_SetRenderDrawColor(game->renderer, 250, 248, 239, 255);
    SDL_RenderClear(game->renderer);

    draw_header(game);
    draw_board(game);

    if (SDL_GetTicks() < game->msg_time) {
        draw_text_centered(game, game->status_msg,
                           0, 650, WINDOW_WIDTH, 50,
                           game->font_small, TEXT_DARK);
    }

    SDL_RenderPresent(game->renderer);
}

void cleanup(Game* game) {
    TTF_CloseFont(game->font_title);
    TTF_CloseFont(game->font_large);
    TTF_CloseFont(game->font_medium);
    TTF_CloseFont(game->font_small);
    SDL_DestroyRenderer(game->renderer);
    SDL_DestroyWindow(game->window);
    TTF_Quit();
    SDL_Quit();
}
