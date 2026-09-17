extern void init_graphics();
extern void draw_keyboard();
extern void update_key_highlight(void);
extern void end_graphics(void);

/* Recoverable conditions only. Fatal ones never come here -- they leave
   graphics mode and print, see fatal() in main.c. */
#define WARN_NONE 0
#define WARN_BAD_PACKET 1
#define WARN_BAD_NOTE 2
#define WARN_NO_FREQ 3
#define WARN_OVERRUN 4
#define WARN_BAD_TYPE 5

extern void graphics_link(uint8_t online);
extern void graphics_warn(uint8_t code);
extern void graphics_warn_tick(void);
