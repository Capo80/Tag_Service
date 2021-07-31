#ifndef BITMAPS_H
#define BITMAPS_H

typedef char* bitmap;

bitmap get_new_bitmap(int size);
void set_bit_one(bitmap bm, int index);
void set_bit_zero(bitmap bm, int index);
bool check_bit(bitmap bm, int index);
void print_bitmap(bitmap bm, int size);
bool check_all_zero(bitmap bm);

#endif