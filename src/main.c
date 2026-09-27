#include <graphics/drawing.h>
#include <graphics/color.h>
#include <graphics/text.h>
#include <graphics/init.h>
#include <graphics/lcdc.h>
#include <sh4a/input/keypad.h>
#include <syscalls/syscalls.h>
#include <stdio.h>

#include "libc/memmgr.h"
#include "ui_jp_font.h"

#define SCREEN_WIDTH 528
#define SCREEN_HEIGHT 320
#define READ_PASSES 1
#define MAX_MODELS 16

static unsigned char io_buffer[8192];
static char model_path[48];
static unsigned long model_drive;
static char model_app_id[10];
static char model_names[MAX_MODELS][64];
static unsigned int model_count;

struct parse_result {
  unsigned int tensors;
  unsigned int int8_tensors;
  unsigned int fp16_tensors;
  unsigned long payload_bytes;
};

static int read_exact(int fd, void *dst, int size) {
  unsigned char *p = (unsigned char *)dst;
  int total = 0;
  while (total < size) {
    int got = sys_read(fd, p + total, size - total);
    if (got <= 0) return -1;
    total += got;
  }
  return 0;
}

static int read_u8(int fd, unsigned int *value) {
  unsigned char b;
  if (read_exact(fd, &b, 1) < 0) return -1;
  *value = b;
  return 0;
}

static int read_u16le(int fd, unsigned int *value) {
  unsigned char b[2];
  if (read_exact(fd, b, 2) < 0) return -1;
  *value = (unsigned int)b[0] | ((unsigned int)b[1] << 8);
  return 0;
}

static int read_u32le(int fd, unsigned long *value) {
  unsigned char b[4];
  if (read_exact(fd, b, 4) < 0) return -1;
  *value = (unsigned long)b[0] | ((unsigned long)b[1] << 8) |
           ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
  return 0;
}

static int discard_bytes(int fd, unsigned long size) {
  while (size > 0) {
    int part = size > sizeof(io_buffer) ? sizeof(io_buffer) : (int)size;
    if (read_exact(fd, io_buffer, part) < 0) return -1;
    size -= part;
  }
  return 0;
}

static int parse_exq12(struct parse_result *out) {
  static const unsigned char magic[8] = {'E','X','Q','1','2',0,0,0};
  unsigned char header_magic[8];
  unsigned long version, count;
  unsigned int t;
  int fd = sys_open(model_path, FILE_RD);

  out->tensors = 0;
  out->int8_tensors = 0;
  out->fp16_tensors = 0;
  out->payload_bytes = 0;
  if (fd < 0) return -10;
  if (read_exact(fd, header_magic, 8) < 0 ||
      memcmp(header_magic, magic, 8) != 0 ||
      read_u32le(fd, &version) < 0 || read_u32le(fd, &count) < 0) {
    sys_close(fd);
    return -11;
  }
  if (version != 1 || count == 0 || count > 128) {
    sys_close(fd);
    return -12;
  }

  for (t = 0; t < count; ++t) {
    unsigned int name_len, ndim, qtype, d;
    unsigned long value, scale_count, data_bytes;
    if (read_u16le(fd, &name_len) < 0 || read_u8(fd, &ndim) < 0 ||
        read_u8(fd, &qtype) < 0 || name_len == 0 || name_len > 80 ||
        ndim == 0 || ndim > 4 || discard_bytes(fd, name_len) < 0) {
      sys_close(fd);
      return -13;
    }
    for (d = 0; d < ndim; ++d)
      if (read_u32le(fd, &value) < 0 || value == 0) {
        sys_close(fd);
        return -14;
      }
    if (read_u32le(fd, &scale_count) < 0 ||
        read_u32le(fd, &data_bytes) < 0 ||
        discard_bytes(fd, scale_count * 4) < 0 ||
        discard_bytes(fd, data_bytes) < 0) {
      sys_close(fd);
      return -15;
    }
    if (qtype == 1) out->int8_tensors++;
    else if (qtype == 3) out->fp16_tensors++;
    else {
      sys_close(fd);
      return -16;
    }
    out->payload_bytes += data_bytes;
    out->tensors++;
  }
  sys_close(fd);
  return 0;
}

static void clear_screen(void) {
  set_pen(create_rgb16(0, 0, 0));
  draw_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static void line(int y, const char *text, unsigned short color) {
  set_pen(color);
  render_text(12, y, text);
}

static void line_jp(int y, const char *text, unsigned short color) {
  set_pen(color);
  render_text_jp(12, y, text);
}

static void show_status(const char *headline, const char *detail,
                        unsigned short color) {
  clear_screen();
  line(18, "EXLLM MODEL CHECK", create_rgb16(0, 255, 255));
  line_jp(52, headline, color);
  line(82, detail, create_rgb16(255, 255, 255));
  line_jp(252, "決定: 再実行", create_rgb16(0, 255, 0));
  line_jp(282, "電源 / 戻る: 終了", create_rgb16(0, 255, 0));
  lcdc_copy_vram();
}

static void discover_models(void) {
  char filename[64];
  int handle, ret;
  unsigned long type;
  model_count = 0;
  ret = sys_findfirst("\\\\drv0\\MODELS\\*.q12", &handle, filename, &type);
  while (ret == 0 && model_count < MAX_MODELS) {
    if (type == 1) {
      strcpy(model_names[model_count], filename);
      model_count++;
    }
    ret = sys_findnext(handle, filename, &type);
  }
  if (ret == 0 || model_count) sys_findclose(handle);
}

static void draw_model_menu(unsigned int selected) {
  unsigned int i, first = selected > 6 ? selected - 6 : 0;
  clear_screen();
  line(18, "EXLLM MODEL CHECK", create_rgb16(0, 255, 255));
  line_jp(50, "モデルを選択", create_rgb16(255, 255, 0));
  if (!model_count) {
    line_jp(90, "MODELSフォルダにモデルがありません", create_rgb16(255, 0, 0));
  } else {
    for (i = first; i < model_count && i < first + 7; ++i)
      line(82 + (int)(i - first) * 28, model_names[i],
           i == selected ? create_rgb16(255, 255, 0) : create_rgb16(210, 210, 210));
  }
  line_jp(282, "上下：選択  決定：検査  戻る：終了", create_rgb16(0, 255, 0));
  lcdc_copy_vram();
}

static int select_model(void) {
  unsigned int selected = 0;
  discover_models();
  draw_model_menu(selected);
  for (;;) {
    keypad_read();
    if (get_key_state(KEY_POWER) || get_key_state(KEY_BACK)) return -1;
    if (model_count && get_key_state(KEY_UP)) {
      while (get_key_state(KEY_UP)) keypad_read();
      selected = selected ? selected - 1 : model_count - 1;
      draw_model_menu(selected);
    }
    if (model_count && get_key_state(KEY_DOWN)) {
      while (get_key_state(KEY_DOWN)) keypad_read();
      selected = (selected + 1) % model_count;
      draw_model_menu(selected);
    }
    if (model_count && get_key_state(KEY_ENTER)) {
      while (get_key_state(KEY_ENTER)) keypad_read();
      sprintf(model_path, "\\\\drv0\\MODELS\\%s", model_names[selected]);
      return (int)selected;
    }
  }
}

static int run_read_test(unsigned long *bytes_read, unsigned long *checksum) {
  int pass;
  unsigned long total = 0;
  unsigned long hash = 2166136261UL;

  show_status("モデルを検査しています", "model.q12 / 5.19 MiB / streaming",
              create_rgb16(255, 255, 0));

  for (pass = 0; pass < READ_PASSES; ++pass) {
    int fd = sys_open(model_path, FILE_RD);
    int expected;
    int current = 0;

    if (fd < 0) return -1;
    expected = sys_get_filesize(fd);
    if (expected < 16) {
      sys_close(fd);
      return -2;
    }

    while (current < expected) {
      int want = expected - current;
      int got;
      int i;
      if (want > (int)sizeof(io_buffer)) want = sizeof(io_buffer);
      got = sys_read(fd, io_buffer, want);
      if (got <= 0) {
        sys_close(fd);
        return -3;
      }
      for (i = 0; i < got; ++i) {
        hash ^= io_buffer[i];
        hash *= 16777619UL;
      }
      current += got;
      total += got;
    }
    sys_close(fd);
  }

  *bytes_read = total;
  *checksum = hash;
  return 0;
}

int main(void *ptr) {
  unsigned long bytes_read = 0;
  unsigned long checksum = 0;
  struct parse_result parsed;
  int result;
  char bytes_line[48];
  char hash_line[48];

  if (ptr != 0 && *(long *)ptr == 1) return -1;

  memmgr_init();
  graphics_init(SCREEN_WIDTH, SCREEN_HEIGHT, (void *)0xAC200000);

  sys_dict_info(&model_drive, model_app_id);

  for (;;) {
    if (select_model() < 0) return -2;
    result = run_read_test(&bytes_read, &checksum);
    if (result == 0) result = parse_exq12(&parsed);
    if (result == 0) {
      sprintf(bytes_line, "Read OK: %u bytes", (unsigned int)bytes_read);
      sprintf(hash_line, "FNV1a-32: %X", (unsigned int)checksum);
      show_status("モデル解析: 正常", bytes_line,
                  create_rgb16(0, 255, 0));
      line(116, hash_line, create_rgb16(255, 255, 255));
      sprintf(bytes_line, "tensors=%u i8=%u f16=%u", parsed.tensors,
              parsed.int8_tensors, parsed.fp16_tensors);
      line(150, bytes_line, create_rgb16(255, 255, 0));
      sprintf(bytes_line, "payload=%u bytes", (unsigned int)parsed.payload_bytes);
      line(184, bytes_line, create_rgb16(255, 255, 255));
      line(218, parsed.tensors == 39 ? "EXLLM runtime layout: compatible" : "EXLLM runtime layout: check required",
           parsed.tensors == 39 ? create_rgb16(0, 255, 0) : create_rgb16(255, 255, 0));
      lcdc_copy_vram();
    } else if (result == -1) {
      sprintf(bytes_line, "drv=%lu id=%s", model_drive, model_app_id);
      show_status("モデルを開けません", model_path,
                  create_rgb16(255, 0, 0));
      line(116, bytes_line, create_rgb16(255, 255, 255));
      lcdc_copy_vram();
    } else if (result == -2) {
      show_status("モデルサイズが不正です", "File is smaller than EXQ12 header",
                  create_rgb16(255, 0, 0));
    } else if (result == -3) {
      show_status("モデル読込エラー", "Short read from internal storage",
                  create_rgb16(255, 0, 0));
    } else {
      sprintf(bytes_line, "Parser error: %d", result);
      show_status("モデル形式エラー", bytes_line,
                  create_rgb16(255, 0, 0));
    }

    for (;;) {
      keypad_read();
      if (get_key_state(KEY_POWER) || get_key_state(KEY_BACK)) return -2;
      if (get_key_state(KEY_ENTER)) {
        while (get_key_state(KEY_ENTER)) keypad_read();
        break;
      }
    }
  }
}
