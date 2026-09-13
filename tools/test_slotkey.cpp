// slotKeyFor: the nested-folder rule. Build with tools\run_tests.bat.
#include "streaming/gate.h"
#include <cassert>
#include <cstdio>
#include <string>

static std::string key(const char* rel) { const char* why = nullptr; return slotKeyFor(rel, &why); }
static bool refused(const char* rel) { const char* why = nullptr; return slotKeyFor(rel, &why).empty() && why; }

int main()
{
    // 1. flat layout: keys unchanged
    assert(key("mp_m_freemode_01/teef_004_u.ydd") == "mp_m_freemode_01/teef_004_u.ydd");
    assert(key("caninesd/head_000_r.ydd") == "caninesd/head_000_r.ydd");
    assert(key("mp_fm_skin_m_up_whi.ytd") == "mp_fm_skin_m_up_whi.ytd");
    assert(key("w_pi_pistol.ydr") == "w_pi_pistol.ydr");
    assert(key("a_c_husky.ydd") == "a_c_husky.ydd");
    // 2. one organiser folder above the collection
    assert(key("clothingpack1/mp_f_freemode_01_female_heist/uppr_013_r.ydd") == "mp_f_freemode_01_female_heist/uppr_013_r.ydd");
    assert(key("packs/2024/mp_f_freemode_01_mp_f_gtawclothes1/jbib_diff_001_a_uni.ytd") == "mp_f_freemode_01_mp_f_gtawclothes1/jbib_diff_001_a_uni.ytd");
    // 3. loose types anywhere
    assert(key("weapons/w_pi_pistol.ydr") == "w_pi_pistol.ydr");
    assert(key("props/box/prop_beer_bottle.ydr") == "prop_beer_bottle.ydr");
    assert(key("anims/capper@nyck.ycd") == "capper@nyck.ycd");
    assert(key("tattoos/mp_sum2_tat_051.ytd") == "mp_sum2_tat_051.ytd");
    assert(key("animals/a_c_husky.ydd") == "a_c_husky.ydd");
    // 4. still refused
    assert(refused("cs_bradcadaver/head_000_r.ydd"));          // story ped folder
    assert(refused("clothingpack1/random.ydd"));              // .ydd with no collection
    assert(refused("animals/a_c_shepherd.ymt"));              // vanilla animal .ymt, nested too
    assert(refused("somefolder/notes.txt"));
    // 5. a ped part directly in an organiser folder keeps today's meaning (the folder IS the collection)
    assert(key("caninesp/head_diff_000_a_whi.ytd") == "caninesp/head_diff_000_a_whi.ytd");
    // 6. disabled folders: the prefix, any case, folder names only
    assert(isDisabledFolder("disabledPack1"));
    assert(isDisabledFolder("DISABLED_pack2"));
    assert(isDisabledFolder("disabled"));
    assert(!isDisabledFolder("Pack2"));
    assert(!isDisabledFolder("mydisabledpack"));
    assert(!isDisabledFolder("mp_m_freemode_01"));
    // 7. _override folders: the copy that wins a duplicate
    assert(isOverridePath("feseroadditionalpack_override/mp_m_freemode_01/feet_007_u.ydd"));
    assert(isOverridePath("packs/fesero_override/w_pi_pistol.ydr"));
    assert(isOverridePath("mp_m_freemode_01_override/feet_007_u.ydd"));
    assert(!isOverridePath("feseroclothingpack/mp_m_freemode_01/feet_007_u.ydd"));
    assert(!isOverridePath("something_override.ytd"));          // a file, not a folder
    assert(!isOverridePath("overrides/w_pi_pistol.ydr"));       // no underscore, not the suffix
    // the suffix never changes which slot a file lands on
    assert(key("feseroadditionalpack_override/mp_m_freemode_01/feet_007_u.ydd") == "mp_m_freemode_01/feet_007_u.ydd");
    assert(key("feseroadditionalpack_override/mp_m_freemode_01/feet_diff_007_b_uni.ytd") == "mp_m_freemode_01/feet_diff_007_b_uni.ytd");
    assert(key("mp_m_freemode_01_override/feet_007_u.ydd") == "mp_m_freemode_01/feet_007_u.ydd");
    assert(key("caninesd_override/head_000_r.ydd") == "caninesd/head_000_r.ydd");
    assert(key("pack_override/w_pi_pistol.ydr") == "w_pi_pistol.ydr");
    assert(refused("cs_bradcadaver_override/head_000_r.ydd"));  // still a story ped
    puts("gate: 7 groups passed");
    return 0;
}
