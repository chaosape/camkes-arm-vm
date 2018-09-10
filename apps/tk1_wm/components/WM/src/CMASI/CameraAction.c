
#include <stdlib.h>
#include <inttypes.h>
#include "common/struct_defines.h"
#include "common/conv.h"
#include "CameraAction.h"
#include "PayloadAction.h"
void lmcp_pp_CameraAction(CameraAction* s) {
    printf("CameraAction{");
    printf("Inherited from PayloadAction:\n");
    lmcp_pp_PayloadAction(&(s->super));
    printf("HorizontalFieldOfView: ");
    printf("%f",s->HorizontalFieldOfView);
    printf("\n");
    printf("}");
}
size_t lmcp_packsize_CameraAction (CameraAction* i) {
    size_t out = 0;
    out += lmcp_packsize_PayloadAction(&(i->super));
    out += sizeof(float);
    return out;
}
size_t lmcp_pack_CameraAction_header(uint8_t* buf, CameraAction* i) {
    uint8_t* outb = buf;
    if (i == NULL) {
        lmcp_pack_uint8_t(outb, 0);
        return 1;
    }
    outb += lmcp_pack_uint8_t(outb, 1);
    memcpy(outb, "CMASI", 5);
    outb += 5;
    for (size_t j = 5; j < 8; j++, outb++)
        *outb = 0;
    outb += lmcp_pack_uint32_t(outb, 18);
    outb += lmcp_pack_uint16_t(outb, 3);
    return 15;
}
void lmcp_free_CameraAction(CameraAction* out, int out_malloced) {
    if (out == NULL)
        return;
    lmcp_free_PayloadAction(&(out->super), 0);
    if (out_malloced == 1) {
        free(out);
    }
}
void lmcp_init_CameraAction (CameraAction** i) {
    if (i == NULL) return;
    (*i) = calloc(1,sizeof(CameraAction));
    ((lmcp_object*)(*i)) -> type = 18;
}
int lmcp_unpack_CameraAction(uint8_t** inb, size_t *size_remain, CameraAction* outp) {
    if (inb == NULL || *inb == NULL) {
        return -1;
    }
    if (size_remain == NULL || *size_remain == 0) {
        return -1;
    }
    CameraAction* out = outp;
    CHECK(lmcp_unpack_PayloadAction(inb, size_remain, &(out->super)))
    CHECK(lmcp_unpack_float(inb, size_remain, &(out->HorizontalFieldOfView)))
    return 0;
}
size_t lmcp_pack_CameraAction(uint8_t* buf, CameraAction* i) {
    if (i == NULL) return 0;
    uint8_t* outb = buf;
    outb += lmcp_pack_PayloadAction(outb, &(i->super));
    outb += lmcp_pack_float(outb, i->HorizontalFieldOfView);
    return (outb - buf);
}
