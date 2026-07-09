/*
 * video.c — Driver de Pixel Buffer VGA para DE10-Standard
 *
 * Baseado na técnica de endereçamento validada em hardware real
 * (ver comentário detalhado em video.h).
 */

#include "video.h"

/* ============================================================
 *  Estado interno de calibração de hardware
 * ============================================================ */
static int screen_x;      /* Largura real detectada                  */
static int screen_y;      /* Altura real detectada                   */
static int res_offset;    /* 1 se resolução real for 160 (metade)    */
static int col_offset;    /* 1 se profundidade de cor real for 8-bit */
static int video_color_bits = 16;

/* ============================================================
 *  Detecção de hardware
 * ============================================================ */
static int get_data_bits(int mode) {
    switch (mode) {
        case 0x0:  return 1;
        case 0x7:  return 8;
        case 0x11: return 8;
        case 0x12: return 9;
        case 0x14: return 16;
        case 0x17: return 24;
        case 0x19: return 30;
        case 0x31: return 8;
        case 0x32: return 12;
        case 0x33: return 16;
        case 0x37: return 32;
        case 0x39: return 40;
        default:   return 16;
    }
}

/* Converte cor RGB 24-bit (0xRRGGBB) para o formato real do hardware.
 * Para 8-bit: empacota o byte de cor duas vezes no short, porque cada
 * escrita de 16 bits nesse modo cobre 2 pixels físicos adjacentes. */
static short resample_rgb(int num_bits, int color) {
    int packed;
    if (num_bits == 8) {
        packed = (((color >> 16) & 0x000000E0) | ((color >> 11) & 0x0000001C) |
                  ((color >> 6)  & 0x00000003));
        packed = (packed << 8) | packed;
    } else if (num_bits == 16) {
        packed = (((color >> 8) & 0x0000F800) | ((color >> 5) & 0x000007E0) |
                  ((color >> 3) & 0x0000001F));
    } else {
        packed = color;
    }
    return (short)packed;
}

void video_init(void) {
    volatile int *video_resolution = (int *)(PIXEL_BUF_CTRL_BASE + 0x8);
    volatile int *rgb_status       = (int *)(RGB_RESAMPLER_BASE);

    screen_x = (*video_resolution) & 0xFFFF;
    screen_y = ((*video_resolution) >> 16) & 0xFFFF;
    video_color_bits = get_data_bits((*rgb_status) & 0x3F);

    /* resolução menor que o padrão 320×240 (ex: 160×120)? */
    res_offset = (screen_x == 160) ? 1 : 0;
    /* profundidade de cor menor que o padrão 16-bit? */
    col_offset = (video_color_bits == 8) ? 1 : 0;
}

/* Desenha um retângulo preenchido usando o stride real do hardware
 * (potência de 2), lendo o ponteiro do framebuffer ativo direto do
 * registrador a cada chamada — sem endereço de back buffer fixo. */
void video_draw_rect(int x, int y, int w, int h, int color) {
    int pixel_buf_ptr = *(int *)PIXEL_BUF_CTRL_BASE;
    int x_shift = res_offset + col_offset; /* fator de ajuste = 1<<x_shift */
    int y_shift = res_offset;
    short packed = resample_rgb(video_color_bits, color);
    int x1, y1, x2, y2, row, col, pixel_ptr;

    /* Shift em vez de divisão: mno-hw-div deixa divisão MUITO lenta
     * no Nios II sem multiplicador/divisor de hardware. Como o fator
     * é sempre potência de 2, shift é matematicamente equivalente
     * e não depende de hardware nenhum. */
    x1 = x >> x_shift;
    y1 = y >> y_shift;
    x2 = (x + w - 1) >> x_shift;
    y2 = (y + h - 1) >> y_shift;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;

    for (row = y1; row <= y2; row++) {
        for (col = x1; col <= x2; col++) {
            pixel_ptr = pixel_buf_ptr
                       + (row << (10 - res_offset - col_offset))
                       + (col << 1);
            *(short *)pixel_ptr = packed;
        }
    }
}

void video_draw_rect_outline(int x, int y, int w, int h, int color) {
    video_draw_rect(x,         y,         w, 1, color); /* topo    */
    video_draw_rect(x,         y + h - 1, w, 1, color); /* base    */
    video_draw_rect(x,         y,         1, h, color); /* esquerda*/
    video_draw_rect(x + w - 1, y,         1, h, color); /* direita */
}

void video_clear(int color) {
    video_draw_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color);
}

/* ============================================================
 *  Texto — usa o buffer de caracteres dedicado do hardware
 *  (não desenha pixels manualmente, evita bugs de fonte)
 * ============================================================ */
void video_text(int col, int row, const char *text) {
    volatile char *character_buffer = (char *)FPGA_CHAR_BASE;
    int offset = (row << 7) + col;
    while (*text) {
        *(character_buffer + offset) = *text;
        text++;
        offset++;
    }
}

void video_clear_text(int col, int row, int width) {
    volatile char *character_buffer = (char *)FPGA_CHAR_BASE;
    int offset = (row << 7) + col;
    int i;
    for (i = 0; i < width; i++)
        *(character_buffer + offset + i) = ' ';
}
