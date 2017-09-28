#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>

#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/hashtable.h>
#include <linux/radix-tree.h>
#include <linux/bitmap.h>

static char* int_str = "";


/*****************************/
/* some supporting functions */
/*****************************/
static int count_spaces(char* str)
{
	int spaces = 0;
	int i = 0;

	if(str == NULL) return -1;	

	for(i = 0; str[i] != '\0'; ++i)
	{
		if(str[i] == ' ') ++spaces;
	}
	return spaces;
}


/*
 *	@fn is_numeric()
 *		determines if a character is an ascii numeral - in the range ['0' , '9']	
 *
 *	@param	x
 *		the character to test
 *
 *	@return
 *		true if x is a numeral
 *		false otherwise
 */
static int is_numeric(char x)
{
	return (x >= '0' && x <= '9') ? true: false;
}

/*
 *	@fn p2_atoi()
 *		converts an ascii string (or space-delimited subsection thereof)
 *			to an integer
 *
 * 	@param	char*	str
 *		the string to convert to integer
 *		this parameter is not altered
 *
 *	@param 	int*	pos
 *		the position of the integer in the input string
 *		this parameter is updated every time this function is called
 *
 *	@return 
 *		an integer equal to the value read from the string, on success
 *		-1 on failure
 */
static int p2_atoi(char* str, int* pos)
{
	int retval = 0;
	int i = 0;
	
	if(str == NULL) return -1;

	while(str[i] != ' ' && str[i] != '\0')
	{
		if(unlikely(is_numeric(str[i]) == false)) return -1;
		retval *= 10;
		retval += str[i] - '0';
		++i;
	}

	//if we care about updating the position, do that here
	if(pos != NULL) *pos = i + 1;

	return retval;
}

/****************************************/
/*	some module init things 	*/
/****************************************/
module_param(int_str, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(int_str, "Space-delimited string containing an arbitrary number of integers with values between 0 and 1000.");


/********************************/
/*	main module code 	*/
/********************************/

/*
 *	@fn p2_linked_list_fn()
 *		creates a linked list from buf and prints the integers to the kernel log
 *	
 * 	@param int* buf
 *		the integer buffer to store and print
 *
 *	@param num_ints
 *		the number of integers in buf
 *
 *	@return
 *		0, always
 */
static int p2_linked_list_fn(int* buf, int num_ints)
{
	struct mystruct{
		int data;
		struct list_head mylist;
	};
	
	int i = 0;
	struct mystruct* listptr = NULL;
	LIST_HEAD(mylinkedlist);
	struct list_head* pos = NULL;
	struct list_head* n = NULL;

	printk("building linked list\n");

	for(i = 0; i < num_ints; ++i)
	{
		listptr = kcalloc(1, sizeof(struct mystruct), GFP_KERNEL);
		listptr->data = buf[i];
		list_add( &listptr->mylist, &mylinkedlist);
	}

	list_for_each_entry(listptr, &mylinkedlist, mylist)
	{
		printk("list data = %d, ", listptr->data);
	}

	printk("freeing linked list\n");

	list_for_each_safe(pos, n, &mylinkedlist)
	{	
		listptr = list_entry(pos, struct mystruct, mylist);
		kfree(listptr);
		listptr = NULL;
	}

	return 0;
}

/**/
/*	rb_tree stuff	*/
/**/
struct myrbstruct{
	int data;
	struct rb_node mytree;
};

static int p2_rb_insert(struct rb_root* root, struct myrbstruct* new)
{
	struct rb_node** link = &root->rb_node;
	struct rb_node*  parent = NULL;
	struct myrbstruct* comp_node = NULL;

	while(*link)
	{
		parent = *link;
		comp_node = rb_entry(parent, struct myrbstruct, mytree);

		if(comp_node->data == new->data)
			return -1;
			/*break out without inserting if the new node is equal to an existing one*/
		else if(comp_node->data > new->data)
			link = &(*link)->rb_left;
		else
			link = &(*link)->rb_right;
	}

	rb_link_node(&new->mytree, parent, link);
	rb_insert_color(&new->mytree, root);

	return 0;
}


/*
 *	@fn p2_rbtree_fn()
 *		creates an rbtree from buf and prints the integers to the kernel log
 *	
 * 	@param int* buf
 *		the integer buffer to store and print
 *
 *	@param num_ints
 *		the number of integers in buf
 *
 *	@return
 *		0, always
 *
 */
static int p2_rbtree_fn(int* buf, int num_ints)
{
	int i = 0;
	struct myrbstruct* rbptr = NULL;
	struct rb_root myrbroot = RB_ROOT;
	struct rb_node* pos = NULL;
	//struct rb_node* n = NULL;

	printk("building rb tree\n");
	
	for(i = 0; i < num_ints; ++i)
	{
		rbptr = kcalloc(1, sizeof(struct myrbstruct), GFP_KERNEL);
		rbptr->data = buf[i];
		p2_rb_insert(&myrbroot, rbptr);
	}

	for(pos = rb_first(&myrbroot); pos != NULL /*rb_last(&myrbroot)*/; pos = rb_next(pos))
	{
		rbptr = rb_entry(pos, struct myrbstruct, mytree);
		printk("rb data = %d\n", rbptr->data);
	}

	printk("freeing rb tree\n");
	
	for(pos = rb_first(&myrbroot); pos != NULL /*rb_last(&myrbroot)*/; pos = rb_next(pos))
	{
		rbptr = rb_entry(pos, struct myrbstruct, mytree);
		rb_erase(pos, &myrbroot);
		kfree(rbptr);
	}

	return 0;
}


/**/
/*	hash table stuff	*/
/**/


static DEFINE_HASHTABLE(htable, 14);

static int p2_hashtable_fn(int* buf, int num_ints)
{
	struct myhashstruct
	{
		int data;
		struct hlist_node myhashlist;
	};
	int i = 0;	
	struct myhashstruct* hptr = NULL;
	//struct hlist_node* nodeptr = NULL;
	hash_init(htable);
	

	//using data as the hash key

	printk("building hash table\n");

	for(i = 0; i < num_ints; ++i)
	{
		hptr = kcalloc(1, sizeof(struct myhashstruct), GFP_KERNEL);
		hptr->data = buf[i];
		hash_add(htable, &hptr->myhashlist, buf[i]);
	}

	//i = 0;
	printk("iterating over htable\n");
	hash_for_each(htable, i, hptr, myhashlist)
	{
		printk("hash data %d\n", hptr->data);	
	}

	printk("looking up over htable\n");

	for(i = 0; i < num_ints; ++i)
	{
		hash_for_each_possible(htable, hptr, myhashlist, buf[i])
		{
			if(hptr->data == buf[i])
				printk("hash data %d\n",hptr->data); 
		}
	}

	printk("destroying hash table\n");
	
	//i = 0;
	hash_for_each(htable, i, hptr, myhashlist)
	{
		hash_del(&hptr->myhashlist);
		kfree(hptr);
	}
	
	return 0;
}



/**/
/*	radix tree stuff	*/
/**/
static int p2_radix_tree_fn(int* buf, int num_ints)
{
	#define P2_ODD_TAG	1
	int i = 0;
	int j = 0;
	int* data = NULL;
	RADIX_TREE(radtree, GFP_KERNEL);
	int** gang = kcalloc(num_ints, sizeof(int*), GFP_KERNEL);

	printk("building radix tree and tagging odd numbers\n");
	for(i = 0; i < num_ints; ++i)
	{
		radix_tree_insert(&radtree, buf[i], &buf[i]);
		if(buf[i] % 2 == 1) /* tag odd entries */
		{
			printk("tagging %d\n", buf[i]);
			radix_tree_tag_set(&radtree, buf[i], P2_ODD_TAG);
		}
	}
	
	printk("printing all numbers\n");
	for(i = 0; i < num_ints; ++i)
	{
		data = radix_tree_lookup(&radtree, buf[i]);
		if(data != NULL)
			printk("radix data: %d\n", *data);
	}

	printk("printing tagged numbers\n");
	//for(i = 0; i < 10; ++i){
		radix_tree_gang_lookup_tag(&radtree, (void**)gang, 0, 
						num_ints, P2_ODD_TAG);
		for(j = 0; j < num_ints && gang[j] != NULL; ++j)
		{
			printk("tag data: %d\n", *gang[j]);
		}
	//}

	printk("freeing radix tree\n");
	for(i = 0; i < num_ints; ++i)
	{
		radix_tree_delete(&radtree, buf[i]);
	}	

	kfree(gang);

	return 0;
}



/**/
/*	bitmap stuff	*/
/**/
static int p2_bitmap_fn(int* buf, int num_ints)
{
	int i = 0;
	DECLARE_BITMAP(bmap, 1000);
	bitmap_zero(bmap, 1000);

	printk("building bitmap\n");
	for(i = 0; i < num_ints; i++)
	{
		set_bit(buf[i], bmap);
	}

	//i = 0;
	for_each_set_bit(i, bmap, 1000)
	{
		printk("bit %d\n", i);
	}

	return 0;
}



/**/
/*	"main" part of module	*/
/**/

static int __init p2_init(void)
{
	int* buf = NULL;
	char* str = &int_str[0];
	int pos = 0;
	int i = 0;	

	//debug logging
	printk(KERN_INFO "Module loaded: 'p2' \n");
	printk("Argument int_str = %s\n", int_str);

	buf = (int*) kcalloc((count_spaces(str)+1), sizeof(int), GFP_KERNEL);

	while(str[0] != '\0')
	{
		buf[i] = p2_atoi(str, &pos);
		if(unlikely(buf[i] < 0))
		{
			//handle error
			printk("int_str contains non-digit characters. Aborting.\n");
			goto p2_init_cleanup;
		}
		else if(unlikely(buf[i] > 1000))
		{
			printk("integer '%d' is out of range. Aborting.\n", buf[i]);
			goto p2_init_cleanup;
		}
		str += pos;
		++i;
	}

	//data structure manipulation
	p2_linked_list_fn(buf, i);
	p2_rbtree_fn(buf, i);
	p2_hashtable_fn(buf, i);
	p2_radix_tree_fn(buf, i);
	p2_bitmap_fn(buf, i);

	p2_init_cleanup:
	printk("Module finished working: 'p2'. Use the command 'sudo rmmod p2' to unload the module. \n");
	kfree(buf);
	return 0;	
}

static void __exit p2_exit(void)
{
	printk(KERN_INFO "Module exiting: 'p2' \n");
}

module_init(p2_init);
module_exit(p2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Monger <mpm8128@vt.edu>");
MODULE_DESCRIPTION("LKP Project 2 kernel module");
