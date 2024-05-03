(module $fiber_wasmfx
  (type $ft1 (func (param i32) (result i32)))
  (type $ft2 (func (param i32 i32) (result i32)))
  (type $ct1 (cont $ft1))
  (type $ct2 (cont $ft2))

  ;; We must make sure that there is only a single memory, so that our
  ;; load/store instructions act on the C heap, not a separate memory.
  (import "benchmark" "memory" (memory $0 2))
  ;; This is created by clang to translate function pointers.
  (import "benchmark" "__indirect_function_table" (table $indirect_function_table 0 funcref))


  ;; Keep the initial size of this table in sync with INITIAL_TABLE_CAPACITY in
  ;; .c file.
  (table $conts 1024 (ref null $ct1))


  (tag $yield (param i32) (result i32))

  ;; Discriminants of the fiber_result_t enum defined in fiber.h.
  ;; Must keep in sync.
  (global $fiber_result_t_ok i32 (i32.const 0))
  (global $fiber_result_t_yield i32 (i32.const 1))


  (func $grow_cont_table (export "wasmfx_grow_cont_table") (param $capacity_delta i32)
    (table.grow $conts (ref.null $ct1) (local.get $capacity_delta))
    (drop)
  )

  ;; This function is the entry point of all of our continuations.
  ;; clang translates function pointers into indices into the
  ;; `$indirect_function_table`, and the latter contains entries of type
  ;; `funcref`. There is no way to downcast from `funcref` to a concrete function
  ;; reference type. Thus we cannot call `cont.new` on entries from
  ;; `$indirect_function_table` directly, but must use this trampoline instead.
  (func $wasmfx_entry_trampoline (param $func_index i32) (param $arg i32) (result i32)
    (call_indirect $indirect_function_table (type $ft1)
      (local.get $arg)
      (local.get $func_index)
    )
  )
  (elem declare func $wasmfx_entry_trampoline)


  (func $indexed_cont_new (export "wasmfx_indexed_cont_new")
    (param $func_index i32)
    (param $cont_index i32)
    (local $cont (ref $ct1))

    (local.get $func_index)
    (cont.new $ct2 (ref.func $wasmfx_entry_trampoline))
    (cont.bind $ct2 $ct1)
    (local.set $cont)

    (table.set $conts (local.get $cont_index) (local.get $cont))
  )

  (func $indexed_resume (export "wasmfx_indexed_resume")
    (param $cont_index i32)
    (param $arg i32)
    (param $result_ptr i32)
    (result i32)
    (local $k (ref $ct1))


    (block $handler (result i32 (ref $ct1) )
      ;; put continuation argument on stack
      (local.get $arg)
      ;; put continuation itself on stack
      (table.get $conts (local.get $cont_index))
      (resume $ct1 (tag $yield $handler))
      (i32.store (local.get $result_ptr) (global.get $fiber_result_t_ok))
      (return) ;; returns the value put on stack by resume
    )
    (local.set $k)

    ;; stash continuation aside
    (table.set $conts (local.get $cont_index) (local.get $k))

    (i32.store (local.get $result_ptr) (global.get $fiber_result_t_yield))
    ;; return suspend payload
    (return)
  )

  (func $suspend (export "wasmfx_suspend") (param $arg i32) (result i32)
    (suspend $yield (local.get $arg))
  )
)
