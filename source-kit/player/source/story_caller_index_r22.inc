/* Original SUB17266's type3 query: low-byte zero returns current9800;
 * otherwise107F4 indexes98E0 from its newest caller, returningFFFF on miss.
 * Pending writes/expression frames do not belong to that original call list. */
static unsigned story_caller_at(unsigned operand){
    unsigned index=operand&255u;
    if(!index)return command_root&65535u;
    for(unsigned i=stack_count;i;i--){
        const Frame *f=stack+i-1;
        if(f->write==2 && !--index)return f->key&65535u;
    }
    return 65535u;
}
