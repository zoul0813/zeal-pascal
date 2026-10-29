"""Host C views of the exact packed assets used by the Zeal compiler."""

def write_assets(root,destination):
    runtime=(root/'build/pascal_runtime.zpl').read_bytes()
    private=(root/'build/pascal_runtime.private').read_bytes()
    profile=(root/'build/pascal_runtime.profile').read_bytes()
    destination.write_text('const unsigned char runtime_start[] = {' +
        ','.join(str(x) for x in runtime+private+profile) + '};\n' +
        '__asm__(".globl _runtime_end\\n_runtime_end = _runtime_start + ' + str(len(runtime)) + '");\n')
    with destination.open('a') as asset:
        asset.write('__asm__(".globl _runtime_private\\n_runtime_private = _runtime_start + '+str(len(runtime))+'");\n')
        for start,end,name in (('file_start','file_end','file_runtime.packed'),('file_gate_start','file_gate_end','file_gate.zpl'),('unpack_start','unpack_end','module_unpack.zpl')):
            data=(root/'build'/name).read_bytes()
            asset.write('const unsigned char '+start+'[] = {'+','.join(map(str,data))+'};\n')
            asset.write('__asm__(".globl _'+end+'\\n_'+end+' = _'+start+' + '+str(len(data))+'");\n')
        data=b''.join((root/'build'/name).read_bytes()for name in ('transfer_runtime.packed','transfer_gate.zpl','transfer_call.zpl','execute_entry.zpl'))
        asset.write('const unsigned char transfer_start[] = {'+','.join(map(str,data))+'};\n')
