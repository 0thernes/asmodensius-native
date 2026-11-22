#include "az_test.h"
#include "../tools/waste_provenance.h"

int main(void)
{
    AztWasteHistory h = {0};
    /* Ten field units enter a pool also holding 100 internally generated units. */
    azt_waste_gain(&h,10);
    azt_waste_drain(&h,110,20,true);
    u64 lo,hi;
    azt_waste_votes(&h,&lo,&hi);
    CHECK_EQ(lo,0); CHECK_EQ(hi,1);
    CHECK_EQ(h.external_min.low,0); CHECK_EQ(h.external_max.low,10);
    /* Converting the entire remainder fixes the total without fixing which
     * conversion contained those field units. */
    azt_waste_drain(&h,90,90,true);
    azt_waste_votes(&h,&lo,&hi);
    CHECK_EQ(lo,1); CHECK_EQ(hi,2);
    CHECK_EQ(h.external_min.low,10); CHECK_EQ(h.external_max.low,10);
    CHECK_EQ(h.pool_min,0); CHECK_EQ(h.pool_max,0);
    /* Repeated loss through a different reaction must leave no external source
     * that a later internal conversion could receive credit for. */
    azt_waste_gain(&h,15);
    azt_waste_drain(&h,15,5,false);
    azt_waste_drain(&h,10,10,false);
    azt_waste_drain(&h,9,9,true);
    CHECK_EQ(h.external_min.low,10); CHECK_EQ(h.external_max.low,10);
    CHECK_EQ(h.conversions,3); CHECK_EQ(h.converted.low,119);
    return az_test_done();
}
