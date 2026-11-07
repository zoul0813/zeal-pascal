.module runtime_object_entry
.globl _runtime_probe,_runtime_workspace
.globl s__INITIALIZER,l__INITIALIZER,s__INITIALIZED
.area _HEADER
    ld l,#<(_runtime_workspace+3)
    ld h,#>(_runtime_workspace+3)
    ld hl,#s__INITIALIZER
    ld de,#s__INITIALIZED
    ld bc,#l__INITIALIZER
    ldir
    jp _runtime_probe
