#include <linux/slab.h>

typedef char* bitmap;

// return a new bitmap of "size" bytes, allocated to 0
bitmap get_new_bitmap(int size) {

	int i;
	bitmap new_bitmap = kmalloc(size, GFP_KERNEL);
	for (i = 0; i < size; i++)
		new_bitmap[i] = 0;

	return new_bitmap;
}

// THE FUNCTIONS DO NOT CHECK IF THE INDEX IS OUTSIDE THE BITMAP
// USE WITH CARE

//set value of bitmap at "index" to 1
void set_bit_one(bitmap bm, int index) {

	int step = index / 8;
	int pos = index % 8;
	
	bm[step] |= 0x1 << pos;
}

//set value of bitmap at "index" to 0
void set_bit_zero(bitmap bm, int index) {

	int step = index / 8;
	int pos = index % 8;
	
	bm[step] &= ~(0x1 << pos);

}

//checks value of bitmap at "index"
bool check_bit(bitmap bm, int index) {

	int step = index / 8;
	int pos = index % 8;

	return (bm[step] >> pos) & 0x1; 
}

bool check_all_zero(bitmap bm) {
	return bm == 0;
}

void print_bitmap(bitmap bm, int size) {

	int i;
	char* to_print = kmalloc(size, GFP_KERNEL);

	for (i = 0; i < size; i++) {
		if (check_bit(bm, i))
			to_print[i] = '1';
		else
			to_print[i] = '0';
	}

	printk("TAG_SERVICE: %s\n", to_print);
	kfree(to_print);
}

void free_bitmap(bitmap bm) {
	kfree(bm);
}