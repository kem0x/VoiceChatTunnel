#pragma once
#include "framework.h"

namespace Resources
{
#include "failedSound.h"
#include "materialIcons.h"
#include "successSound.h"

#include "Rank_0.h"
#include "Rank_1.h"
#include "Rank_10.h"
#include "Rank_11.h"
#include "Rank_12.h"
#include "Rank_13.h"
#include "Rank_14.h"
#include "Rank_15.h"
#include "Rank_16.h"
#include "Rank_17.h"
#include "Rank_18.h"
#include "Rank_19.h"
#include "Rank_2.h"
#include "Rank_20.h"
#include "Rank_21.h"
#include "Rank_22.h"
#include "Rank_23.h"
#include "Rank_24.h"
#include "Rank_3.h"
#include "Rank_4.h"
#include "Rank_5.h"
#include "Rank_6.h"
#include "Rank_7.h"
#include "Rank_8.h"
#include "Rank_9.h"

    const std::vector<std::pair<unsigned char*, size_t>> rankBuffers {
        { (unsigned char*)&Rank_0, sizeof(Rank_0) },
        { (unsigned char*)&Rank_1, sizeof(Rank_1) },
        { (unsigned char*)&Rank_2, sizeof(Rank_2) },
        { (unsigned char*)&Rank_3, sizeof(Rank_3) },
        { (unsigned char*)&Rank_4, sizeof(Rank_4) },
        { (unsigned char*)&Rank_5, sizeof(Rank_5) },
        { (unsigned char*)&Rank_6, sizeof(Rank_6) },
        { (unsigned char*)&Rank_7, sizeof(Rank_7) },
        { (unsigned char*)&Rank_8, sizeof(Rank_8) },
        { (unsigned char*)&Rank_9, sizeof(Rank_9) },
        { (unsigned char*)&Rank_10, sizeof(Rank_10) },
        { (unsigned char*)&Rank_11, sizeof(Rank_11) },
        { (unsigned char*)&Rank_12, sizeof(Rank_12) },
        { (unsigned char*)&Rank_13, sizeof(Rank_13) },
        { (unsigned char*)&Rank_14, sizeof(Rank_14) },
        { (unsigned char*)&Rank_15, sizeof(Rank_15) },
        { (unsigned char*)&Rank_16, sizeof(Rank_16) },
        { (unsigned char*)&Rank_17, sizeof(Rank_17) },
        { (unsigned char*)&Rank_18, sizeof(Rank_18) },
        { (unsigned char*)&Rank_19, sizeof(Rank_19) },
        { (unsigned char*)&Rank_20, sizeof(Rank_20) },
        { (unsigned char*)&Rank_21, sizeof(Rank_21) },
        { (unsigned char*)&Rank_22, sizeof(Rank_22) },
        { (unsigned char*)&Rank_23, sizeof(Rank_23) },
        { (unsigned char*)&Rank_24, sizeof(Rank_24) }
    };
};
