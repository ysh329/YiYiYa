/*******************************************************************
 * Copyright 2021-2080 evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/
#include "mouse.h"

#include "kernel/kernel.h"
#include "pic.h"

mouse_device_t mouse_device;

static size_t read(device_t* dev, void* buf, size_t len) {
  u32 ret = len;
  mouse_device_t* data=buf;
  *data=mouse_device;
  return ret;
}

INTERRUPT_SERVICE
void mouse_handler() {
  interrupt_entering_code(ISR_MOUSE, 0);
  interrupt_process(do_mouse);
  interrupt_exit();
}

u8 mouse_read() {
  mouse_wait(0);
  return io_read8(MOUSE_DATA);
}

void mouse_write(u8 data) {
  mouse_wait(1);
  io_write8(MOUSE_COMMAND, 0xD4);
  mouse_wait(1);
  io_write8(MOUSE_DATA, data);
}

void mouse_wait(u8 type) {
  u32 time_out = 1000000;
  for (; time_out > 0; time_out--) {
    if ((io_read8(MOUSE_STATUS) & 1 + type) == 1) {
      break;
    }
  }
}

int mouse_init(void) {
  device_t* dev = kmalloc(sizeof(device_t));
  dev->name = "mouse";
  dev->read = read;
  dev->id = DEVICE_MOUSE;
  dev->type = DEVICE_TYPE_CHAR;
  dev->data = &mouse_device;

  device_add(dev);
  interrutp_regist(ISR_MOUSE, mouse_handler);

  mouse_device.x = 0;
  mouse_device.y = 0;

  io_write8(MOUSE_COMMAND,
            0xa8);  // Enable second PS/2 port (only if 2 PS/2 ports supported)
  mouse_wait(1);

  io_write8(MOUSE_COMMAND, 0x20);  // Read "byte 0" from internal RAM

  u8 status = (io_read8(MOUSE_DATA) | 2);
  mouse_wait(1);
  io_write8(MOUSE_COMMAND, 0x60);  // Write next byte to "byte 0" of internal
                                   // RAM (Controller Configuration Byte

  io_write8(MOUSE_DATA, status);

  mouse_write(0xf6);  // Set Defaults
  mouse_read();

  mouse_write(0xf4);  // Enable Data Reporting
  mouse_read();

  pic_enable(ISR_MOUSE);

  return 0;
}

void do_mouse(interrupt_context_t* context) {
  u32 read_count = 0;
  u8 state = 0;
  for (; io_read8(MOUSE_STATUS) & 1;) {
    u8 data = io_read8(MOUSE_DATA);
    if (read_count == 0) {
      mouse_device.sate = data;
      state = data;
    } else if (read_count == 1) {
      mouse_device.x = mouse_device.x + data - ((state << 4) & 0x100);
    } else if (read_count == 2) {
      mouse_device.y = mouse_device.y + data - ((state << 3) & 0x100);
      
      if (mouse_device.x < 0) {
        mouse_device.x = 0;
      }
      if (mouse_device.y < 0) {
        mouse_device.y = 0;
      }
    }
    read_count = (read_count + 1) % 3;
  }

  pic_eof(ISR_MOUSE);
}

void mouse_exit(void) { kprintf("mouse exit\n"); }

module_t mouse_module = {
    .name = "mouse", .init = mouse_init, .exit = mouse_exit};
module_regit(mouse_module);