#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/stacktrace.h>
#include <linux/rbtree.h>
#include <linux/kallsyms.h>
#include <asm/msr.h>

static const char* filename = "lattop";
static int tree_elements;
static char namebuf[128];

static struct rb_root p3_rbroot = RB_ROOT;
static long long int ns_since_boot;
static long long int cc_over_ns_ratio;

//using fixed-point precision for ns->cc conversion since floating point isn't 
//	an option
//using '1000' specifically because it's small enough that it'll basically 
//	never overflow, while still large enough to be useful.
#define fixedpoint_adj	1000

#define max_print 1000

struct myrbstruct{
	char* name;
	int pid;
	unsigned long long sleep_cc;
	unsigned long long sleep_ns;
	struct stack_trace *stack;
	struct rb_node mytree;
};

static int p3_l_greater_than_r(struct myrbstruct* old, struct myrbstruct* new)
{
	
	if(old->sleep_ns == new->sleep_ns)
	{
		//if the two sleep times are somehow the same, compare the pids
		//	instead - those should never be the same.
		return (old->pid > new->pid);
	}
	else if(old->sleep_ns > new->sleep_ns) return 1;
	else /*(old->sleep_ns < new->sleep_ns)*/ return 0;
}

static int p3_rb_insert(struct rb_root* root, struct myrbstruct* new)
{
	struct rb_node** link = &root->rb_node;
	struct rb_node*  parent = NULL;
	struct myrbstruct* comp_node = NULL;

	while(*link)
	{
		parent = *link;
		comp_node = rb_entry(parent, struct myrbstruct, mytree);
		
		if(comp_node == NULL) return -1;
		
		else if(p3_l_greater_than_r(comp_node, new))
			link = &(*link)->rb_left;
		else
			link = &(*link)->rb_right;
	}

	rb_link_node(&new->mytree, parent, link);
	rb_insert_color(&new->mytree, root);

	return 0;
}

static void p3_destroy_tree(void)
{
	struct rb_node *pos = NULL;
	struct myrbstruct *rbptr = NULL;

	printk("destroy_tree starting\n");

	for(pos = rb_first(&p3_rbroot); pos != NULL; pos = rb_first(&p3_rbroot))
	{
		rbptr = rb_entry(pos, struct myrbstruct, mytree);
		if(rbptr)
		{
			if(rbptr->stack)
				kfree(rbptr->stack->entries);
			kfree(rbptr->stack);
			kfree(rbptr);
		}
		rb_erase(pos, &p3_rbroot);
	}
	tree_elements = 0;
}



static int p3_build_tree(void)
{
	struct task_struct *task = NULL;
	struct myrbstruct *rbptr = NULL;
	struct stack_trace* strace = NULL;
	int i = 0;

	printk("build_tree starting\n");

	printk("building rb tree\n");

	rcu_read_lock();
	for_each_process(task)
	{
		rbptr = kcalloc(1, sizeof(struct myrbstruct), GFP_KERNEL);
		if(rbptr == NULL)
			goto cleanup_rbptr;
		rbptr->name = task->comm;
		rbptr->pid = task->pid;	

		strace = kcalloc(1, sizeof(struct stack_trace), GFP_KERNEL);
		if(strace == NULL) 
			goto cleanup_strace;

		strace->nr_entries = 0;
		strace->max_entries = 10;
		strace->skip = 0;		

		strace->entries = kcalloc(strace->max_entries, sizeof(long), GFP_KERNEL);
		if(strace->entries == NULL) 
			goto cleanup_entries;
		
		save_stack_trace_tsk(task, strace);	
		rbptr->stack = strace;
		
		//ns since boot - start timestamp == ns since started
		//ns since started - ns running == ns sleeping
		rbptr->sleep_ns = ns_since_boot;
		rbptr->sleep_ns -= task->start_time;
		rbptr->sleep_ns -= task->se.sum_exec_runtime;
		
		//convert to clock cycles at the end, make adjustment for the
		//	fixed-point arithmetic
		rbptr->sleep_cc = rbptr->sleep_ns * fixedpoint_adj / cc_over_ns_ratio;
		if(p3_rb_insert(&p3_rbroot, rbptr))
			goto cleanup_insert;
		++i;
	}
	rcu_read_unlock();
	tree_elements = i;
	printk("tree_elements = %d\n", tree_elements);
	//success
	return 0;
	
	//
	//failure	

	//clean and free everything in reverse order to allocation
	cleanup_insert:
	printk("insertion failed: cleanup insert\n");
	kfree(strace->entries);

	cleanup_entries:
	printk("allocation failed: cleanup entries\n");
	kfree(strace);
	
	cleanup_strace:
	printk("allocation failed: cleanup strace\n");
	kfree(rbptr);
	
	cleanup_rbptr:
	printk("allocation failed: cleanup rbptr\n");
	p3_destroy_tree();

	return -1;
}

static void* p3_seq_start(struct seq_file *s, loff_t *pos)
{
	int i = 0;
	struct rb_node *treepos = NULL;
	printk("seq_start starting\n");

	if(pos == NULL) 
	{
		printk("seq_start ending: pos == NULL\n");
		return NULL;
	}
	printk("seq_start: *pos = %lld\n", *pos);
	if(*pos >= max_print)
	{
		printk("seq_start ending: pos >= 1000\n");
		return NULL;
	}
	if(*pos >= tree_elements)
	{
		printk("seq_start ending: pos >= tree_elements\n");
		return NULL;
	}
	if(*pos == 0) return rb_last(&p3_rbroot);
	
	for(treepos = rb_last(&p3_rbroot), i = 0; treepos != NULL; 
				treepos = rb_prev(treepos), ++i)
	{
		if(i == *pos) break;
	}
	return treepos;
}

static void* p3_seq_next(struct seq_file *s, void* v, loff_t *pos)
{
	// start at last element in rb tree
	//	advance *(pos) to next element
	//	end iteration after 1000 elements
	struct rb_node *spos = NULL;

	printk("seq_next starting: v = %p, *pos = %lld", v, *pos);

	if(pos == NULL) return NULL;
	++(*pos);

	spos = rb_prev(v);

	return spos;
}


static void p3_seq_stop(struct seq_file *s, void *v)
{
	printk("seq_stop starting: v = %p\n", v);
	//p3_destroy_tree();
}

static int p3_seq_show(struct seq_file *s, void* v)
{
	// print out contents of the struct
	//	for the current element of the rb tree	
	struct rb_node *spos = v;
	struct myrbstruct *rbptr = NULL;
	int i = 0;

	printk("seq_show starting: v = %p\n", v);

	if(v == NULL)
		return -1;

	rbptr = rb_entry(spos, struct myrbstruct, mytree);

	if(rbptr == NULL)
		return -1;

	seq_printf(s, "process: %s, pid: %d\n", rbptr->name, rbptr->pid);
	seq_printf(s, "sleep time (clock cycles): %llu, (ns): %llu\n", rbptr->sleep_cc, rbptr->sleep_ns);

	seq_printf(s, "stack trace:\n");
	for(i = 0; i < rbptr->stack->nr_entries; ++i)
	{
		if(sprint_symbol(namebuf, rbptr->stack->entries[i]) < 0)
		{
			//basic error handling
			namebuf[0] = 0;
			continue;
		}
		seq_printf(s, "%lx, %s\n", rbptr->stack->entries[i], namebuf);
	}
	seq_printf(s, "\n");

	return 0;
}

static const struct seq_operations p3_seq_ops = {
	.start = p3_seq_start,
	.next = p3_seq_next,
	.stop = p3_seq_stop,
	.show = p3_seq_show
};

static int p3_open(struct inode *inode, struct file *file)
{
	int retval = 0;
	long long int cc_since_boot = 0;
	struct timespec *curr_time = NULL;

	curr_time = kcalloc(1, sizeof(struct timespec), GFP_KERNEL);
	if(curr_time == NULL)
		return -1;

	printk("p3_open called\n");
	
	//we probably won't get interrupted here, but it'd break things if we
	//	did - so we disable preemption while we're getting time info
	//	and building the tree.
	preempt_disable();
	
	//sleep_ns = current time - boot time == ns since boot
	getnstimeofday(curr_time);
	ns_since_boot = timespec_to_ns(curr_time);
	getboottime64(curr_time);
	ns_since_boot -= timespec_to_ns(curr_time);

	//get clock cycles since boot, then get cc/ns ratio to convert sleep
	//	times in p3_build_tree()
	cc_since_boot = rdtsc();
	cc_over_ns_ratio = (cc_since_boot * fixedpoint_adj) / (ns_since_boot);

	retval = p3_build_tree();	
	preempt_enable();
	
	kfree(curr_time);
	curr_time = NULL;

	if( retval != 0 )
	{
		printk("seq_start ending: build tree failed\n");
		return -1;
	}
	return seq_open(file, &p3_seq_ops);
}

static int p3_release(struct inode *inode, struct file *file)
{
	printk("p3_release starting\n");
	p3_destroy_tree();
	return seq_release(inode, file);
}

static struct file_operations proc_fops = {
	.owner = THIS_MODULE,
	.open = p3_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = p3_release
};


static int __init p3_init(void)
{
	printk("Loading module: p3\n");
	printk("adding procfs entry %s\n", filename);
	
	proc_create(filename, 0, NULL, &proc_fops);
	
	tree_elements = 0;
	ns_since_boot = 0;	
	cc_over_ns_ratio = 0;

	return 0;
}

static void __exit p3_exit(void)
{
	//p3_destroy_tree();
	printk("removing procfs entry %s\n", filename);
	remove_proc_entry(filename, NULL);

	printk("Unloading module p3\n");
}

module_init(p3_init);
module_exit(p3_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Monger <mpm8128@vt.edu>");
MODULE_DESCRIPTION("Latency profiler");
