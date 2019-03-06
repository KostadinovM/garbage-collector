#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define STACK_MAX 256
#define INITIAL_GC_THRESHOLD 10

//enum to define an object's type
typedef enum 
{
	OBJ_INT,
	OBJ_PAIR
} ObjectType;

//this struct identifies whether an object is an int or a pair
typedef struct sObject 
{
	//keep track of all objects that have been allocated
	struct sObject* next;

	//flag to check if object for garbage collection
	unsigned char marked;

	ObjectType type;

	union 
	{
		int value; //OBJ_INT

		struct 
		{
			struct sObject* head;
			struct sObject* tail;
		};
	};
} Object;

//virtual machine struct - in order to have a stack that stores variables in scope
typedef struct 
{
	//total number of currently allocated objects
	int numObjects;

	//the number of objects required to trigger a GC call
	int maxObjects;

	//keeps track of the first object that was allocated
	Object* firstObject;

	Object* stack[STACK_MAX];
	int stackSize;
} VM;

void gc(VM* vm);

//func that creates and initializes a new Virtual Machine
VM* newVM() 
{
	VM* vm = malloc(sizeof(VM));
	vm->stackSize = 0;
	vm->firstObject = 0;

	vm->numObjects = 0;
	vm->maxObjects = INITIAL_GC_THRESHOLD;
	return vm;
}

//stack manipulation - add to the stack
void push(VM *vm, Object *value) 
{
	assert(vm->stackSize < STACK_MAX, "STACK OVERFLOW!");
	vm->stack[vm->stackSize++] = value;
}

//stack manipulation - remove from the stack
Object* pop(VM* vm) 
{
	assert(vm->stackSize > 0, "Stack underflow!");
	return vm->stack[--vm->stackSize];
}

//create objects - handle memory allocation and object type
Object* newObject(VM* vm, ObjectType type) 
{
	if (vm->numObjects == vm->maxObjects)
		gc(vm);

	Object *object = malloc(sizeof(Object));
	object->type = type;

	//add it to the list of allocated objects
	object->next = vm->firstObject;
	vm->firstObject = object;
	object->marked = 0;

	vm->numObjects++;
	return object;
}

//push an INT onto the VM
void pushInt(VM* vm, int intValue) 
{
	Object* object = newObject(vm, OBJ_INT);
	object->value = intValue;
	push(vm, object);
}

//push a pair onto the VM
Object* pushPair(VM* vm)
{
	Object* object = newObject(vm, OBJ_PAIR);
	object->tail = pop(vm);
	object->head = pop(vm);

	push(vm, object);
	return object;
}

//mark a single object for garbage collection
void mark(Object* object)
{
	//check if object has already been marked to avoid inifnite looping
	if (object->marked)
		return;

	object->marked = 1;

	if (object->type == OBJ_PAIR)
	{
		mark(object->head);
		mark(object->tail);
	}
}

//mark all objects for gabage collection
void markAll(VM* vm)
{
	for (int i = 0; i < vm->stackSize; i++)
	{
		mark(vm->stack[i]);
	}
}

void sweep(VM* vm)
{
	Object** object = &vm->firstObject;
	while (*object)
	{
		if (!(*object)->marked)
		{
			//object wasnt reached, so remove it from the list and free it
			Object* unreached = *object;
			*object = unreached->next;
			free(unreached);
			vm->numObjects--;
		}
		else
		{
			//object was reached, so we unmark it(for next GC) and move on to the next
			(*object)->marked = 0;
			object = &(*object)->next;
		}
	}
}

//call the gc
void gc(VM* vm)
{
	int numObjects = vm->numObjects;

	markAll(vm);
	sweep(vm);

	vm->maxObjects = vm->numObjects * 2;

	printf("Collected %d objects, %d remaining.\n", numObjects - vm->numObjects,
		vm->numObjects);
}

void freeVM(VM *vm) {
	vm->stackSize = 0;
	gc(vm);
	free(vm);
}

void test_preserve() {
	printf("Test 1: Objects on stack are preserved.\n");
	VM* vm = newVM();
	pushInt(vm, 1);
	pushInt(vm, 2);

	gc(vm);
	assert(vm->numObjects == 2, "Should have preserved objects.");
	freeVM(vm);
}

void test_collect() {
	printf("Test 2: Unreached objects are collected.\n");
	VM* vm = newVM();
	pushInt(vm, 1);
	pushInt(vm, 2);
	pop(vm);
	pop(vm);

	gc(vm);
	assert(vm->numObjects == 0, "Should have collected objects.");
	freeVM(vm);
}

void test_reach() {
	printf("Test 3: Reach nested objects.\n");
	VM* vm = newVM();
	pushInt(vm, 1);
	pushInt(vm, 2);
	pushPair(vm);
	pushInt(vm, 3);
	pushInt(vm, 4);
	pushPair(vm);
	pushPair(vm);

	gc(vm);
	assert(vm->numObjects == 7, "Should have reached objects.");
	freeVM(vm);
}

void test_handle() {
	printf("Test 4: Handle cycles.\n");
	VM* vm = newVM();
	pushInt(vm, 1);
	pushInt(vm, 2);
	Object* a = pushPair(vm);
	pushInt(vm, 3);
	pushInt(vm, 4);
	Object* b = pushPair(vm);

	/* Set up a cycle, and also make 2 and 4 unreachable and collectible. */
	a->tail = b;
	b->tail = a;

	gc(vm);
	assert(vm->numObjects == 4, "Should have collected objects.");
	freeVM(vm);
}

int main(int argc, const char * argv[]) {
	test_preserve();
	test_collect();
	test_reach();
	test_handle();

	return 0;
}