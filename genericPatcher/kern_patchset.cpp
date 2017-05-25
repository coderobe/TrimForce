//
//  kern_patchset.cpp
//
//  Copyright © 2017 coderobe. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <Library/LegacyIOService.h>

#include <mach/vm_map.h>
#include <IOKit/IORegistryEntry.h>

#include "kern_patchset.hpp"

KernelVersion KernelCheck = getKernelVersion();

static const char* kextPaths[] {
    "/System/Library/Extensions/IOAHCIFamily.kext/Contents/PlugIns/IOAHCIBlockStorage.kext/Contents/MacOS/IOAHCIBlockStorage",
};

static KernelPatcher::KextInfo kextList[] {
    { "com.apple.iokit.IOAHCIBlockStorage", &kextPaths[0], 1, true, {}, KernelPatcher::KextInfo::Unloaded },
};

template <typename T,unsigned S>
inline unsigned arraysize(const T (&v)[S]) { return S; }
static size_t kextListSize = arraysize(kextList);

bool PatchSet::init() {
	LiluAPI::Error error = lilu.onKextLoad(kextList, kextListSize,
    [](void* user, KernelPatcher& patcher, size_t index, mach_vm_address_t address, size_t size) {
        PatchSet* patchset = static_cast<PatchSet*>(user);
		patchset->processKext(patcher, index, address, size);
	}, this);
	
	if(error != LiluAPI::Error::NoError) {
		SYSLOG("coderobe.TrimForce: failed to register onPatcherLoad method %d", error);
		return false;
	}
	
	return true;
}

void PatchSet::deinit() {
}

void PatchSet::processKext(KernelPatcher& patcher, size_t index, mach_vm_address_t address, size_t size){
    if(progressState != ProcessingState::EverythingDone) {
        for(size_t i = 0; i < kextListSize; i++) {
            if(kextList[i].loadIndex == index) {
                if(!(progressState & ProcessingState::EverythingDone) && !strcmp(kextList[i].id, kextList[0].id)) {
                    SYSLOG("coderobe.TrimForce: found %s", kextList[i].id);
                    
                    const uint8_t find[]    = {0x00, 0x41, 0x50, 0x50, 0x4c, 0x45, 0x20, 0x53, 0x53, 0x44, 0x00};
                    const uint8_t replace[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                    KextPatch kext_patch {
                        {&kextList[i], find, replace, sizeof(find), 2},
                        KernelVersion::Sierra, KernelVersion::Sierra
                    };
                    applyPatches(patcher, index, &kext_patch, 1);
                    progressState |= ProcessingState::EverythingDone;
                    SYSLOG("coderobe.TrimForce: patch applied");
                }
            }
        }
    }
	patcher.clearError();
}

void PatchSet::applyPatches(KernelPatcher& patcher, size_t index, const KextPatch* patches, size_t patchNum) {
    for (size_t p = 0; p < patchNum; p++) {
        auto &patch = patches[p];
        if (patch.patch.kext->loadIndex == index) {
            if (patcher.compatibleKernel(patch.minKernel, patch.maxKernel)) {
                SYSLOG("coderobe.TrimForce: patching %s (%ld/%ld)...", patch.patch.kext->id, p+1, patchNum);
                patcher.applyLookupPatch(&patch.patch);
                patcher.clearError();
            }
        }
    }
}

