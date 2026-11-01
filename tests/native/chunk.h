/* Private selector for independent native test components. Stage retains its
 * original RAM/saved-execution meaning in harness.c. Missing means legacy all. */
volatile uint8_t test_group,test_groups[8];
static uint8_t chunk_select(uint8_t count) {
    int8_t fd=open("H:/SUITE.GRP",O_RDONLY);
    uint8_t value[2]={0,0};uint16_t bytes=2,failures=test_failed;
    if(fd<0)return 1;
    check(read(fd,value,&bytes)==0&&bytes==1);
    check(close(fd)==0);
    check(value[0]<=count);
    test_group=value[0];
    return test_failed==failures;
}
static uint8_t chunk_wants(uint8_t group) {
    return !test_group||test_group==group;
}
