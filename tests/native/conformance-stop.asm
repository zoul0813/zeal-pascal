.module corpus_checkpoint
.globl _corpus_checkpoint
.globl _corpus_data_page
.area _TEXT
_corpus_checkpoint::
    ret
.area _HOME
_corpus_data_page::
    .db 0
