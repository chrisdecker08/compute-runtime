/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/os_interface/os_library.h"

#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/os_interface/windows/wddm_memory_manager_tests.h"
#include "runtime/gmm_helper/gmm_helper.h"

using namespace OCLRT;
using namespace ::testing;

void WddmMemoryManagerFixture::SetUp() {
    MemoryManagementFixture::SetUp();
    WddmFixture::SetUp();
    ASSERT_NE(nullptr, wddm);
    if (platformDevices[0]->capabilityTable.ftrCompression) {
        GMM_DEVICE_CALLBACKS dummyDeviceCallbacks = {};
        GMM_TRANSLATIONTABLE_CALLBACKS dummyTTCallbacks = {};
        wddm->resetPageTableManager(GmmPageTableMngr::create(&dummyDeviceCallbacks, 0, &dummyTTCallbacks));
    }
}

TEST(WddmMemoryManagerAllocator32BitTest, allocator32BitIsCreatedWithCorrectBase) {
    WddmMock *wddm = static_cast<WddmMock *>(Wddm::createWddm());
    uint64_t base = 0x56000;
    uint64_t size = 0x9000;
    wddm->setHeap32(base, size);

    std::unique_ptr<WddmMemoryManager> mm = std::unique_ptr<WddmMemoryManager>(new WddmMemoryManager(false, wddm));

    ASSERT_NE(nullptr, mm->allocator32Bit.get());

    EXPECT_EQ(base, mm->allocator32Bit->getBase());
}

TEST(WddmMemoryManagerWithDeferredDeleterTest, givenWMMWhenAsyncDeleterIsEnabledAndWaitForDeletionsIsCalledThenDeleterInWddmIsSetToNullptr) {
    WddmMock *wddm = new WddmMock();
    bool actualDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(true);
    MockWddmMemoryManager memoryManager(wddm);
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.waitForDeletions();
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(actualDeleterFlag);
}

HWTEST_F(WddmMemoryManagerTest, AllocateAndFree) {
    SetUpMm<FamilyType>();
    auto *ptr = mm->allocateGraphicsMemory(0x1000);
    EXPECT_NE(nullptr, ptr);
    mm->freeGraphicsMemory(ptr);
}

HWTEST_F(WddmMemoryManagerTest, givenDefaultWddmMemoryManagerWhenAskedForVirtualPaddingSupportThenFalseIsReturned) {
    SetUpMm<FamilyType>();
    EXPECT_FALSE(mm->peekVirtualPaddingSupport());
}

HWTEST_F(WddmMemoryManagerTest, AllocateGpuMemHostPtr) {
    SetUpMm<FamilyType>();
    // three pages
    void *ptr = alignedMalloc(3 * 4096, 4096);
    ASSERT_NE(nullptr, ptr);

    auto *gpuAllocation = mm->allocateGraphicsMemory(0x1000, ptr);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ(ptr, gpuAllocation->getUnderlyingBuffer());

    mm->freeGraphicsMemory(gpuAllocation);
    alignedFree(ptr);
}

HWTEST_F(WddmMemoryManagerTest, givenDefaultMemoryManagerWhenAllocateWithSizeIsCalledThenResourceHandleIsZero) {
    SetUpMm<FamilyType>();
    auto *gpuAllocation = mm->allocateGraphicsMemory(0x1000, MemoryConstants::pageSize);

    auto wddmAllocation = static_cast<WddmAllocation *>(gpuAllocation);

    EXPECT_EQ(0u, wddmAllocation->resourceHandle);

    mm->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleIsCalledThenNonNullGraphicsAllocationIsReturned) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = (void *)0x1000;

    std::unique_ptr<Gmm> gmm(Gmm::create(pSysMem, 4096u, false));
    auto status = setSizesFunction(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = mm->createGraphicsAllocationFromSharedHandle(osHandle, false);
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(RESOURCE_HANDLE, wddmAlloc->resourceHandle);
    EXPECT_EQ(ALLOCATION_HANDLE, wddmAlloc->handle);

    mm->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromNTHandleIsCalledThenNonNullGraphicsAllocationIsReturned) {
    SetUpMm<FamilyType>();
    auto size = 4096u;
    void *pSysMem = (void *)0x1000;

    std::unique_ptr<Gmm> gmm(Gmm::create(pSysMem, 4096u, false));
    auto status = setSizesFunction(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = mm->createGraphicsAllocationFromNTHandle((void *)1);
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(NT_RESOURCE_HANDLE, wddmAlloc->resourceHandle);
    EXPECT_EQ(NT_ALLOCATION_HANDLE, wddmAlloc->handle);

    mm->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenLockUnlockIsCalledThenReturnPtr) {
    SetUpMm<FamilyType>();
    auto alloc = mm->allocateGraphicsMemory(1);

    auto ptr = mm->lockResource(alloc);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(1u, mockWddm->lockResult.called);
    EXPECT_TRUE(mockWddm->lockResult.success);

    mm->unlockResource(alloc);
    EXPECT_EQ(1u, mockWddm->unlockResult.called);
    EXPECT_TRUE(mockWddm->unlockResult.success);

    mm->freeGraphicsMemory(alloc);
}

HWTEST_F(WddmMemoryManagerTest, createAllocationFromSharedHandleReturns32BitAllocWhenForce32bitAddressingIsSetAndRequireSpecificBitnessIsTrue) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = (void *)0x1000;

    std::unique_ptr<Gmm> gmm(Gmm::create(pSysMem, 4096u, false));
    auto status = setSizesFunction(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    mm->setForce32BitAllocations(true);

    auto *gpuAllocation = mm->createGraphicsAllocationFromSharedHandle(osHandle, true);
    ASSERT_NE(nullptr, gpuAllocation);
    if (is64bit) {
        EXPECT_TRUE(gpuAllocation->is32BitAllocation);

        uint64_t base = mm->allocator32Bit->getBase();
        EXPECT_EQ(Gmm::canonize(base), gpuAllocation->gpuBaseAddress);
    }

    mm->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, createAllocationFromSharedHandleDoesNotReturn32BitAllocWhenForce32bitAddressingIsSetAndRequireSpecificBitnessIsFalse) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = (void *)0x1000;

    std::unique_ptr<Gmm> gmm(Gmm::create(pSysMem, 4096u, false));
    auto status = setSizesFunction(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    mm->setForce32BitAllocations(true);

    auto *gpuAllocation = mm->createGraphicsAllocationFromSharedHandle(osHandle, false);
    ASSERT_NE(nullptr, gpuAllocation);

    EXPECT_FALSE(gpuAllocation->is32BitAllocation);
    if (is64bit) {
        uint64_t base = 0;
        EXPECT_EQ(base, gpuAllocation->gpuBaseAddress);
    }

    mm->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenFreeAllocFromSharedHandleIsCalledThenDestroyResourceHandle) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = (void *)0x1000;

    std::unique_ptr<Gmm> gmm(Gmm::create(pSysMem, 4096u, false));
    auto status = setSizesFunction(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto gpuAllocation = (WddmAllocation *)mm->createGraphicsAllocationFromSharedHandle(osHandle, false);
    EXPECT_NE(nullptr, gpuAllocation);
    auto expectedDestroyHandle = gpuAllocation->resourceHandle;
    EXPECT_NE(0u, expectedDestroyHandle);

    auto lastDestroyed = getMockLastDestroyedResHandleFcn();
    EXPECT_EQ(0u, lastDestroyed);

    mm->freeGraphicsMemory(gpuAllocation);
    lastDestroyed = getMockLastDestroyedResHandleFcn();
    EXPECT_EQ(lastDestroyed, expectedDestroyHandle);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerSizeZeroWhenCreateFromSharedHandleIsCalledThenUpdateSize) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = (void *)0x1000;

    std::unique_ptr<Gmm> gmm(Gmm::create(pSysMem, size, false));
    auto status = setSizesFunction(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = mm->createGraphicsAllocationFromSharedHandle(osHandle, false);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(size, gpuAllocation->getUnderlyingBufferSize());
    mm->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleFailsThenReturnNull) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = (void *)0x1000;

    std::unique_ptr<Gmm> gmm(Gmm::create(pSysMem, size, false));
    auto status = setSizesFunction(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    mockWddm->failOpenSharedHandle = true;

    auto *gpuAllocation = mm->createGraphicsAllocationFromSharedHandle(osHandle, false);
    EXPECT_EQ(nullptr, gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    SetUpMm<FamilyType>();
    MockContext context;
    context.setMemoryManager(mm);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_TRUE(imageGraphicsAllocation->gmm->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageIsBeingCreatedFromHostPtrThenallocateGraphicsMemoryForImageIsUsed) {
    SetUpMm<FamilyType>();
    MockContext context;
    context.setMemoryManager(mm);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    char data[64u * 64u * 4 * 8];

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_TRUE(imageGraphicsAllocation->gmm->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenNonTiledImgisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    SetUpMm<FamilyType>();
    MockContext context;
    context.setMemoryManager(mm);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;

    char data[64u * 4 * 8];

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->gmm->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_BUFFER);
}

HWTEST_F(WddmMemoryManagerTest, AllocateGpuMemHostPtrOffseted) {
    SetUpMm<FamilyType>();
    WddmAllocation alloc, allocOffseted;
    bool success = false;
    // three pages
    void *ptr = alignedMalloc(4 * 4096, 4096);
    ASSERT_NE(nullptr, ptr);

    size_t baseOffset = 1024;
    // misalligned buffer spanning accross 3 pages
    auto *gpuAllocation = mm->allocateGraphicsMemory(2 * 4096, (char *)ptr + baseOffset);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ((char *)ptr + baseOffset, gpuAllocation->getUnderlyingBuffer());

    auto &hostPtrManager = mm->hostPtrManager;

    auto fragment = hostPtrManager.getFragment(ptr);
    ASSERT_NE(nullptr, fragment);
    EXPECT_TRUE(fragment->refCount == 1);
    EXPECT_NE(fragment->osInternalStorage, nullptr);

    // offseted by 3 pages, not in boundary
    auto fragment2 = hostPtrManager.getFragment((char *)ptr + 3 * 4096);

    EXPECT_EQ(nullptr, fragment2);

    // offseted by one page, still in boundary
    void *offsetedPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + 4096);
    auto *gpuAllocation2 = mm->allocateGraphicsMemory(0x1000, offsetedPtr);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ(offsetedPtr, gpuAllocation2->getUnderlyingBuffer());

    auto fragment3 = hostPtrManager.getFragment(offsetedPtr);
    ASSERT_NE(nullptr, fragment3);

    EXPECT_TRUE(fragment3->refCount == 2);
    EXPECT_EQ(alloc.handle, allocOffseted.handle);
    EXPECT_EQ(alloc.getUnderlyingBufferSize(), allocOffseted.getUnderlyingBufferSize());
    EXPECT_EQ(alloc.getAlignedCpuPtr(), allocOffseted.getAlignedCpuPtr());

    mm->freeGraphicsMemory(gpuAllocation2);

    auto fragment4 = hostPtrManager.getFragment(ptr);
    ASSERT_NE(nullptr, fragment4);

    EXPECT_TRUE(fragment4->refCount == 1);

    mm->freeGraphicsMemory(gpuAllocation);

    fragment4 = hostPtrManager.getFragment(ptr);
    EXPECT_EQ(nullptr, fragment4);

    alignedFree(ptr);
}

HWTEST_F(WddmMemoryManagerTest, AllocateGpuMemCheckGmm) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;
    bool success = false;
    // three pages
    void *ptr = alignedMalloc(3 * 4096, 4096);
    auto *gpuAllocation = mm->allocateGraphicsMemory(3 * 4096, ptr);
    // Should be same cpu ptr and gpu ptr
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(ptr, gpuAllocation->getUnderlyingBuffer());

    auto &hostPtrManager = mm->hostPtrManager;

    auto fragment = hostPtrManager.getFragment(ptr);
    ASSERT_NE(nullptr, fragment);
    EXPECT_TRUE(fragment->refCount == 1);
    EXPECT_NE(fragment->osInternalStorage->handle, 0);
    EXPECT_NE(fragment->osInternalStorage->gmm, nullptr);
    mm->freeGraphicsMemory(gpuAllocation);
    alignedFree(ptr);
}

HWTEST_F(WddmMemoryManagerTest, GivenAlignedPointerWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;
    bool success = false;
    uint32_t size = 4096;
    void *ptr = (void *)4096;
    auto *gpuAllocation = mm->allocate32BitGraphicsMemory(size, ptr);
    EXPECT_EQ(ptr, (void *)gpuAllocation->gmm->resourceParams.pExistingSysMem);
    EXPECT_EQ(size, gpuAllocation->gmm->resourceParams.ExistingSysMemSize);
    mm->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, GivenUnAlignedPointerAndSizeWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;
    bool success = false;
    uint32_t size = 0x1001;
    void *ptr = (void *)0x1001;
    auto *gpuAllocation = mm->allocate32BitGraphicsMemory(size, ptr);
    EXPECT_EQ((void *)0x1000, (void *)gpuAllocation->gmm->resourceParams.pExistingSysMem);
    EXPECT_EQ(0x2000, gpuAllocation->gmm->resourceParams.ExistingSysMemSize);
    mm->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, getSystemSharedMemory) {
    SetUpMm<FamilyType>();
    int64_t mem = mm->getSystemSharedMemory();
    EXPECT_EQ(mem, 4249540608);
}

HWTEST_F(WddmMemoryManagerTest, getMaxApplicationAddress) {
    SetUpMm<FamilyType>();
    uint64_t maxAddr = mm->getMaxApplicationAddress();
    if (is32bit) {
        EXPECT_EQ(maxAddr, MemoryConstants::max32BitAppAddress);
    } else {
        EXPECT_EQ(maxAddr, MemoryConstants::max64BitAppAddress);
    }
}

HWTEST_F(WddmMemoryManagerTest, Allocate32BitMemoryWithNullptr) {
    SetUpMm<FamilyType>();
    auto *gpuAllocation = mm->allocate32BitGraphicsMemory(3 * MemoryConstants::pageSize, nullptr);

    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_LE(Gmm::canonize(wddm->getHeap32Base()), gpuAllocation->getGpuAddress());
    EXPECT_GT(Gmm::canonize(wddm->getHeap32Base()) + wddm->getHeap32Size() - 1, gpuAllocation->getGpuAddress());

    EXPECT_EQ(0u, gpuAllocation->fragmentsStorage.fragmentCount);
    mm->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, Allocate32BitMemoryWithMisalignedHostPtrDoesNotDoTripleAlloc) {
    SetUpMm<FamilyType>();
    size_t misalignedSize = 0x2500;
    void *misalignedPtr = (void *)0x12500;

    auto *gpuAllocation = mm->allocate32BitGraphicsMemory(misalignedSize, misalignedPtr);

    ASSERT_NE(nullptr, gpuAllocation);

    EXPECT_EQ(alignSizeWholePage(misalignedPtr, misalignedSize), gpuAllocation->getUnderlyingBufferSize());

    EXPECT_LE(Gmm::canonize(wddm->getHeap32Base()), gpuAllocation->getGpuAddress());
    EXPECT_GT(Gmm::canonize(wddm->getHeap32Base()) + wddm->getHeap32Size() - 1, gpuAllocation->getGpuAddress());

    EXPECT_EQ(0u, gpuAllocation->fragmentsStorage.fragmentCount);

    void *alignedPtr = alignDown(misalignedPtr, MemoryConstants::allocationAlignment);
    uint64_t offset = ptrDiff(misalignedPtr, alignedPtr);

    EXPECT_EQ(offset, gpuAllocation->allocationOffset);
    mm->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, Allocate32BitMemorySetsCannonizedGpuBaseAddress) {
    SetUpMm<FamilyType>();
    auto *gpuAllocation = mm->allocate32BitGraphicsMemory(3 * MemoryConstants::pageSize, nullptr);

    ASSERT_NE(nullptr, gpuAllocation);

    uint64_t cannonizedAddress = Gmm::canonize(wddm->getHeap32Base());
    EXPECT_EQ(cannonizedAddress, gpuAllocation->gpuBaseAddress);

    mm->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, GivenThreeOsHandlesWhenAskedForDestroyAllocationsThenAllMarkedAllocationsAreDestroyed) {
    SetUpMm<FamilyType>();
    OsHandleStorage storage;
    void *pSysMem = (void *)0x1000;

    storage.fragmentStorageData[0].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[0].residency = new ResidencyData;

    storage.fragmentStorageData[0].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[0].freeTheFragment = true;
    storage.fragmentStorageData[0].osHandleStorage->gmm = Gmm::create(pSysMem, 4096u, false);

    storage.fragmentStorageData[1].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[1].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[1].residency = new ResidencyData;

    storage.fragmentStorageData[1].freeTheFragment = false;

    storage.fragmentStorageData[2].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[2].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[2].freeTheFragment = true;
    storage.fragmentStorageData[2].osHandleStorage->gmm = Gmm::create(pSysMem, 4096u, false);
    storage.fragmentStorageData[2].residency = new ResidencyData;

    mm->cleanOsHandles(storage);

    auto destroyWithResourceHandleCalled = 0u;
    D3DKMT_DESTROYALLOCATION2 *ptrToDestroyAlloc2 = nullptr;

    getSizesFunction(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
    EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);

    EXPECT_EQ(ALLOCATION_HANDLE, storage.fragmentStorageData[1].osHandleStorage->handle);

    delete storage.fragmentStorageData[1].osHandleStorage;
    delete storage.fragmentStorageData[1].residency;
}

HWTEST_F(WddmMemoryManagerTest, freeNullAllocationNoCrash) {
    SetUpMm<FamilyType>();
    mm->freeGraphicsMemory(nullptr);
}

HWTEST_F(WddmMemoryManagerResidencyTest, addToTrimCandidateListPlacesAllocationInContainerAndAssignsPosition) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;

    mm->addToTrimCandidateList(&allocation);

    EXPECT_NE(0u, mm->trimCandidateList.size());
    EXPECT_NE(trimListUnusedPosition, allocation.getTrimCandidateListPosition());

    size_t position = allocation.getTrimCandidateListPosition();
    ASSERT_LT(position, mm->trimCandidateList.size());

    EXPECT_EQ(&allocation, mm->trimCandidateList[position]);
}

HWTEST_F(WddmMemoryManagerResidencyTest, addToTrimCandidateListDoesNotInsertAllocationAlreadyOnTheList) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;

    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(&allocation);

    EXPECT_NE(trimListUnusedPosition, allocation.getTrimCandidateListPosition());

    size_t position = allocation.getTrimCandidateListPosition();
    ASSERT_LT(position, mm->trimCandidateList.size());

    EXPECT_EQ(&allocation, mm->trimCandidateList[position]);

    size_t previousSize = mm->trimCandidateList.size();
    mm->addToTrimCandidateList(&allocation);

    EXPECT_EQ(previousSize, mm->trimCandidateList.size());
    EXPECT_EQ(position, allocation.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, removeFromTrimCandidateListAssignsUnusedPosition) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;

    mm->addToTrimCandidateList(&allocation);
    mm->removeFromTrimCandidateList(&allocation);

    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, removeFromTrimCandidateListRemovesAllocationInAssignedPosition) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;

    mm->addToTrimCandidateList(&allocation);
    size_t position = allocation.getTrimCandidateListPosition();

    mm->removeFromTrimCandidateList(&allocation);

    if (mm->trimCandidateList.size() > position) {
        EXPECT_NE(&allocation, mm->trimCandidateList[position]);
    }
}

HWTEST_F(WddmMemoryManagerResidencyTest, removeFromTrimCandidateListRemovesLastAllocation) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;

    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(&allocation);

    mm->removeFromTrimCandidateList(&allocation);

    EXPECT_EQ(0u, mm->trimCandidateList.size());
}

HWTEST_F(WddmMemoryManagerResidencyTest, removeFromTrimCandidateListRemovesLastAllocationAndAllPreviousEmptyEntries) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2;

    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(&allocation1);

    mm->trimCandidateList.push_back(nullptr);
    mm->trimCandidateList.push_back(nullptr);
    mm->trimCandidateList.push_back(nullptr);

    mm->addToTrimCandidateList(&allocation2);

    EXPECT_EQ(5u, mm->trimCandidateList.size());

    mm->removeFromTrimCandidateList(&allocation2);

    EXPECT_EQ(1u, mm->trimCandidateList.size());
}

HWTEST_F(WddmMemoryManagerResidencyTest, successiveAddingToTrimCandidateListAssignsNewPositions) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3;

    mm->addToTrimCandidateList(&allocation1);
    mm->addToTrimCandidateList(&allocation2);
    mm->addToTrimCandidateList(&allocation3);

    EXPECT_EQ(3u, mm->trimCandidateList.size());
    EXPECT_NE(allocation1.getTrimCandidateListPosition(), allocation2.getTrimCandidateListPosition());
    EXPECT_NE(allocation2.getTrimCandidateListPosition(), allocation3.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, DISABLED_removingNotLastAllocationFromTrimCandidateListSubstituesLastPositionAllocation) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3;

    mm->addToTrimCandidateList(&allocation1);
    mm->addToTrimCandidateList(&allocation2);
    mm->addToTrimCandidateList(&allocation3);

    mm->removeFromTrimCandidateList(&allocation2);

    EXPECT_EQ(2u, mm->trimCandidateList.size());

    EXPECT_EQ(2u, allocation3.getTrimCandidateListPosition());
    EXPECT_NE(allocation2.getTrimCandidateListPosition(), allocation3.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, removingNotLastAllocationFromTrimCandidateListPutsNullEntry) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3;

    mm->addToTrimCandidateList(&allocation1);
    mm->addToTrimCandidateList(&allocation2);
    mm->addToTrimCandidateList(&allocation3);
    size_t position2 = allocation2.getTrimCandidateListPosition();
    size_t position3 = allocation3.getTrimCandidateListPosition();

    mm->removeFromTrimCandidateList(&allocation2);

    EXPECT_EQ(3u, mm->trimCandidateList.size());
    EXPECT_EQ(2u, position3);
    EXPECT_EQ(nullptr, mm->trimCandidateList[position2]);
}

HWTEST_F(WddmMemoryManagerResidencyTest, compactTrimCandidateListRemovesInitialNullEntriesAndUpdatesPositions) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    mm->addToTrimCandidateList(&allocation1);
    mm->addToTrimCandidateList(&allocation2);
    mm->addToTrimCandidateList(&allocation3);
    mm->addToTrimCandidateList(&allocation4);

    size_t position3 = allocation3.getTrimCandidateListPosition();
    size_t position4 = allocation4.getTrimCandidateListPosition();

    mm->removeFromTrimCandidateList(&allocation2);
    mm->removeFromTrimCandidateList(&allocation1);

    EXPECT_EQ(4u, mm->trimCandidateList.size());

    mm->compactTrimCandidateList();

    EXPECT_EQ(2u, mm->trimCandidateList.size());

    EXPECT_EQ(mm->trimCandidateList[0], &allocation3);
    EXPECT_EQ(0u, allocation3.getTrimCandidateListPosition());

    EXPECT_EQ(mm->trimCandidateList[1], &allocation4);
    EXPECT_EQ(1u, allocation4.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, compactTrimCandidateListWithNonNullEntries) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    mm->addToTrimCandidateList(&allocation1);
    mm->addToTrimCandidateList(&allocation2);
    mm->addToTrimCandidateList(&allocation3);
    mm->addToTrimCandidateList(&allocation4);

    EXPECT_EQ(4u, mm->trimCandidateList.size());

    mm->compactTrimCandidateList();

    EXPECT_EQ(4u, mm->trimCandidateList.size());
}

HWTEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsMarksAllocationsResident) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    mm->pushAllocationForResidency(&allocation1);
    mm->pushAllocationForResidency(&allocation2);
    mm->pushAllocationForResidency(&allocation3);
    mm->pushAllocationForResidency(&allocation4);

    mm->makeResidentResidencyAllocations(nullptr);

    EXPECT_TRUE(allocation1.getResidencyData().resident);
    EXPECT_TRUE(allocation2.getResidencyData().resident);
    EXPECT_TRUE(allocation3.getResidencyData().resident);
    EXPECT_TRUE(allocation4.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsUpdatesLastFence) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    mm->pushAllocationForResidency(&allocation1);
    mm->pushAllocationForResidency(&allocation2);
    mm->pushAllocationForResidency(&allocation3);
    mm->pushAllocationForResidency(&allocation4);

    wddm->getMonitoredFence().currentFenceValue = 20;

    mm->makeResidentResidencyAllocations(nullptr);

    EXPECT_EQ(20u, allocation1.getResidencyData().lastFence);
    EXPECT_EQ(20u, allocation2.getResidencyData().lastFence);
    EXPECT_EQ(20u, allocation3.getResidencyData().lastFence);
    EXPECT_EQ(20u, allocation4.getResidencyData().lastFence);
}

HWTEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsMarksTripleAllocationsResident) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2;

    WddmAllocation *allocationTriple = (WddmAllocation *)mm->allocateGraphicsMemory(8196, (void *)0x1500);

    mm->pushAllocationForResidency(&allocation1);
    mm->pushAllocationForResidency(allocationTriple);
    mm->pushAllocationForResidency(&allocation2);

    mm->makeResidentResidencyAllocations(nullptr);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident);
    }

    EXPECT_EQ(5u, gdi.getMakeResidentArg().NumAllocations);

    mm->freeGraphicsMemory(allocationTriple);
}

HWTEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsSetsLastFencePLusOneForTripleAllocations) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2;

    WddmAllocation *allocationTriple = (WddmAllocation *)mm->allocateGraphicsMemory(8196, (void *)0x1500);

    wddm->getMonitoredFence().currentFenceValue = 20;

    mm->pushAllocationForResidency(&allocation1);
    mm->pushAllocationForResidency(allocationTriple);
    mm->pushAllocationForResidency(&allocation2);

    mm->makeResidentResidencyAllocations(nullptr);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_EQ(20u, allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->lastFence);
    }

    mm->freeGraphicsMemory(allocationTriple);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimCallbackIsRegisteredInWddmMemoryManagerCtor) {
    SetUpMm<FamilyType>();
    EXPECT_EQ((PFND3DKMT_TRIMNOTIFICATIONCALLBACK)mm->trimCallback, gdi.getRegisterTrimNotificationArg().Callback);
    EXPECT_EQ((void *)mm, gdi.getRegisterTrimNotificationArg().Context);
    EXPECT_EQ(wddm->getDevice(), gdi.getRegisterTrimNotificationArg().hDevice);
}

HWTEST_F(WddmMemoryManagerResidencyTest, givenNotUsedAllocationsFromPreviousPeriodicTrimWhenTrimResidencyPeriodicTrimIsCalledThenAllocationsAreEvictedMarkedAndRemovedFromTrimCandidateList) {
    SetUpMm<FamilyType>();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // allocations have fence value == 0 by default
    WddmAllocation allocation1, allocation2;
    allocation1.getResidencyData().resident = true;
    allocation2.getResidencyData().resident = true;

    // Set last periodic fence value
    mm->lastPeriodicTrimFenceValue = 10;
    // Set current fence value to greater value
    wddm->getMonitoredFence().currentFenceValue = 20;

    mockWddm->makeNonResidentResult.called = 0;

    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(&allocation1);
    mm->addToTrimCandidateList(&allocation2);

    mm->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 2 allocations evicted
    EXPECT_EQ(2u, mockWddm->makeNonResidentResult.called);
    // removed from trim candidate list
    EXPECT_EQ(0u, mm->trimCandidateList.size());
    // marked nonresident
    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerResidencyTest, givenOneUsedAllocationFromPreviousPeriodicTrimWhenTrimResidencyPeriodicTrimIsCalledThenOneAllocationIsTrimmed) {
    SetUpMm<FamilyType>();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // allocations have fence value == 0 by default
    WddmAllocation allocation1, allocation2;
    allocation1.getResidencyData().resident = true;
    // mark allocation used from last periodic trim
    allocation2.getResidencyData().lastFence = 11;
    allocation2.getResidencyData().resident = true;

    // Set last periodic fence value
    mm->lastPeriodicTrimFenceValue = 10;
    // Set current fence value to greater value
    wddm->getMonitoredFence().currentFenceValue = 20;

    mockWddm->makeNonResidentResult.called = 0;

    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(&allocation1);
    mm->addToTrimCandidateList(&allocation2);

    mm->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 1 allocation evicted
    EXPECT_EQ(1u, mockWddm->makeNonResidentResult.called);
    // removed from trim candidate list
    EXPECT_EQ(trimListUnusedPosition, allocation1.getTrimCandidateListPosition());

    //marked nonresident
    EXPECT_FALSE(allocation1.getResidencyData().resident);
    // second stays resident
    EXPECT_TRUE(allocation2.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerResidencyTest, givenTripleAllocationWithUsedAndUnusedFragmentsSincePreviousTrimWhenTrimResidencyPeriodicTrimIsCalledThenProperFragmentsAreEvictedAndMarked) {
    SetUpMm<FamilyType>();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // 3-fragment Allocation
    WddmAllocation *allocationTriple = (WddmAllocation *)mm->allocateGraphicsMemory(8196, (void *)0x1500);
    // whole allocation unused since previous trim
    allocationTriple->getResidencyData().lastFence = 0;

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->lastFence = 0;
    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident = true;
    // this fragment was used
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->lastFence = 11;
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->resident = true;
    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->lastFence = 0;
    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident = true;

    // Set last periodic fence value
    mm->lastPeriodicTrimFenceValue = 10;
    // Set current fence value to greater value
    wddm->getMonitoredFence().currentFenceValue = 20;

    mockWddm->makeNonResidentResult.called = 0;

    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(allocationTriple);

    mm->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 2 fragments evicted with one call
    EXPECT_EQ(1u, mockWddm->makeNonResidentResult.called);
    // marked nonresident
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident);

    mm->freeGraphicsMemory(allocationTriple);
}

HWTEST_F(WddmMemoryManagerResidencyTest, givenPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    SetUpMm<FamilyType>();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    mm->lastPeriodicTrimFenceValue = 10;
    // Set current fence value to greater value
    *wddm->getMonitoredFence().cpuAddress = 20;

    mm->trimCandidateList.resize(0);
    mm->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, mm->lastPeriodicTrimFenceValue);
}

HWTEST_F(WddmMemoryManagerResidencyTest, givenRestartPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    SetUpMm<FamilyType>();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.RestartPeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    mm->lastPeriodicTrimFenceValue = 10;
    // Set current fence value to greater value
    *wddm->getMonitoredFence().cpuAddress = 20;

    mm->trimCandidateList.resize(0);
    mm->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, mm->lastPeriodicTrimFenceValue);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetWithZeroSizeReturnsTrue) {
    SetUpMm<FamilyType>();
    bool status = mm->trimResidencyToBudget(0);

    EXPECT_TRUE(status);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetAllDoneAllocations) {
    SetUpMm<FamilyType>();
    gdi.setNonZeroNumBytesToTrimInEvict();

    WddmAllocation allocation1, allocation2, allocation3;
    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 0;

    allocation2.getResidencyData().lastFence = 1;
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().lastFence = 2;
    allocation3.getResidencyData().resident = true;

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 1;
    wddm->getMonitoredFence().currentFenceValue = 1;

    mockWddm->makeNonResidentResult.called = 0;

    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(&allocation1);
    mm->addToTrimCandidateList(&allocation2);
    mm->addToTrimCandidateList(&allocation3);

    mm->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(2u, mockWddm->makeNonResidentResult.called);

    EXPECT_EQ(1u, mm->trimCandidatesCount);
    mm->compactTrimCandidateList();
    EXPECT_EQ(1u, mm->trimCandidateList.size());

    EXPECT_EQ(trimListUnusedPosition, allocation1.getTrimCandidateListPosition());
    EXPECT_EQ(trimListUnusedPosition, allocation2.getTrimCandidateListPosition());
    EXPECT_NE(trimListUnusedPosition, allocation3.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetReturnsFalseWhenNumBytesToTrimIsNotZero) {
    SetUpMm<FamilyType>();
    gdi.setNonZeroNumBytesToTrimInEvict();

    WddmAllocation allocation1;
    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 0;

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 1;

    mockWddm->makeNonResidentResult.called = 0;
    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(&allocation1);

    bool status = mm->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, mockWddm->makeNonResidentResult.called);
    EXPECT_EQ(0u, mm->trimCandidateList.size());

    EXPECT_FALSE(status);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetStopsEvictingWhenNumBytesToTrimIsZero) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1((void *)(0x1000), 0x1000, (void *)(0x1000), 0x1000),
        allocation2((void *)(0x1000), 0x3000, (void *)(0x1000), 0x3000),
        allocation3((void *)(0x1000), 0x1000, (void *)(0x1000), 0x1000);

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 0;

    allocation2.getResidencyData().lastFence = 1;
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().lastFence = 2;
    allocation3.getResidencyData().resident = true;

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 1;
    wddm->getMonitoredFence().currentFenceValue = 1;

    mockWddm->makeNonResidentResult.called = 0;

    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(&allocation1);
    mm->addToTrimCandidateList(&allocation2);
    mm->addToTrimCandidateList(&allocation3);

    bool status = mm->trimResidencyToBudget(3 * 4096);

    EXPECT_TRUE(status);
    EXPECT_EQ(2u, mockWddm->makeNonResidentResult.called);
    EXPECT_EQ(1u, mm->trimCandidateList.size());

    EXPECT_EQ(trimListUnusedPosition, allocation1.getTrimCandidateListPosition());
    EXPECT_EQ(trimListUnusedPosition, allocation2.getTrimCandidateListPosition());
    EXPECT_NE(trimListUnusedPosition, allocation3.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetMarksEvictedAllocationNonResident) {
    SetUpMm<FamilyType>();
    gdi.setNonZeroNumBytesToTrimInEvict();

    WddmAllocation allocation1, allocation2, allocation3;
    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 0;

    allocation2.getResidencyData().lastFence = 1;
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().lastFence = 2;
    allocation3.getResidencyData().resident = true;

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 1;
    wddm->getMonitoredFence().currentFenceValue = 1;

    mockWddm->makeNonResidentResult.called = 0;

    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(&allocation1);
    mm->addToTrimCandidateList(&allocation2);
    mm->addToTrimCandidateList(&allocation3);

    bool status = mm->trimResidencyToBudget(3 * 4096);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_TRUE(allocation3.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetWaitsFromCpuWhenLastFenceIsGreaterThanMonitored) {
    SetUpMm<FamilyType>();
    gdi.setNonZeroNumBytesToTrimInEvict();

    WddmAllocation allocation1;
    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 2;

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 2;
    wddm->getMonitoredFence().currentFenceValue = 3;

    mockWddm->makeNonResidentResult.called = 0;
    mockWddm->waitFromCpuResult.called = 0;

    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(&allocation1);

    gdi.getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;

    bool status = mm->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, mockWddm->makeNonResidentResult.called);
    EXPECT_FALSE(allocation1.getResidencyData().resident);

    EXPECT_EQ(mockWddm->getDevice(), gdi.getWaitFromCpuArg().hDevice);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetEvictsDoneFragmentsOnly) {
    SetUpMm<FamilyType>();
    gdi.setNonZeroNumBytesToTrimInEvict();

    WddmAllocation allocation1((void *)0x1000, 0x1000, (void *)0x1000, 0x1000), allocation2((void *)0x1000, 0x1000, (void *)0x1000, 0x1000);
    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 0;

    allocation2.getResidencyData().lastFence = 1;
    allocation2.getResidencyData().resident = true;

    WddmAllocation *allocationTriple = (WddmAllocation *)mm->allocateGraphicsMemory(8196, (void *)0x1500);

    allocationTriple->getResidencyData().lastFence = 1;
    allocationTriple->getResidencyData().resident = true;

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    for (uint32_t i = 0; i < 3; i++) {
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->lastFence = 1;
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident = true;
    }

    // This should not be evicted
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->lastFence = 2;

    mm->trimCandidateList.resize(0);

    mm->addToTrimCandidateList(&allocation1);
    mm->addToTrimCandidateList(allocationTriple);
    mm->addToTrimCandidateList(&allocation2);

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 1;
    wddm->getMonitoredFence().currentFenceValue = 2;

    mockWddm->makeNonResidentResult.called = 0;

    bool status = mm->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(2u, mockWddm->makeNonResidentResult.called);

    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->resident);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident);

    mm->freeGraphicsMemory(allocationTriple);
}

HWTEST_F(WddmMemoryManagerResidencyTest, checkTrimCandidateListCompaction) {
    SetUpMm<FamilyType>();
    mm->trimCandidatesCount = 10;
    mm->trimCandidateList.resize(20);

    bool comapctionRequired = mm->checkTrimCandidateListCompaction();

    EXPECT_TRUE(comapctionRequired);

    mm->trimCandidatesCount = 5;
    mm->trimCandidateList.resize(20);

    comapctionRequired = mm->checkTrimCandidateListCompaction();

    EXPECT_TRUE(comapctionRequired);

    mm->trimCandidatesCount = 18;
    mm->trimCandidateList.resize(20);

    comapctionRequired = mm->checkTrimCandidateListCompaction();

    EXPECT_FALSE(comapctionRequired);
}

HWTEST_F(BufferWithWddmMemory, ValidHostPtr) {
    SetUpMm<FamilyType>();
    flags = CL_MEM_USE_HOST_PTR;

    auto ptr = alignedMalloc(MemoryConstants::preferredAlignment, MemoryConstants::preferredAlignment);

    auto buffer = Buffer::create(
        &context,
        flags,
        MemoryConstants::preferredAlignment,
        ptr,
        retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, buffer);

    auto address = buffer->getCpuAddress();
    EXPECT_EQ(ptr, address);
    EXPECT_NE(nullptr, buffer->getGraphicsAllocation());
    EXPECT_NE(nullptr, buffer->getGraphicsAllocation()->getUnderlyingBuffer());

    delete buffer;
    alignedFree(ptr);
}

HWTEST_F(BufferWithWddmMemory, NullOsHandleStorageAskedForPopulationReturnsFilledPointer) {
    SetUpMm<FamilyType>();
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = (void *)0x1000;
    storage.fragmentStorageData[0].fragmentSize = MemoryConstants::pageSize;
    mm->populateOsHandles(storage);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage->gmm);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    mm->cleanOsHandles(storage);
}

HWTEST_F(BufferWithWddmMemory, GivenMisalignedHostPtrAndMultiplePagesSizeWhenAskedForGraphicsAllcoationThenItContainsAllFragmentsWithProperGpuAdrresses) {
    SetUpMm<FamilyType>();
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = mm->allocateGraphicsMemory(size, ptr);

    auto &hostPtrManager = mm->hostPtrManager;

    ASSERT_EQ(3u, hostPtrManager.getFragmentCount());

    auto reqs = HostPtrManager::getAllocationRequirements(ptr, size);

    for (int i = 0; i < max_fragments_count; i++) {

        uintptr_t GpuPtr = (uintptr_t)(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gpuPtr);
        uintptr_t CpuPtr = (uintptr_t)(reqs.AllocationFragments[i].allocationPtr);
        EXPECT_EQ(CpuPtr, GpuPtr);
        EXPECT_NE((D3DKMT_HANDLE) nullptr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->handle);

        EXPECT_NE(nullptr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gmm);
        EXPECT_EQ(reqs.AllocationFragments[i].allocationPtr,
                  (void *)graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gmm->resourceParams.pExistingSysMem);
        EXPECT_EQ(reqs.AllocationFragments[i].allocationSize,
                  graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gmm->resourceParams.BaseWidth);
    }
    mm->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

HWTEST_F(BufferWithWddmMemory, GivenPointerAndSizeWhenAskedToCreateGrahicsAllocationThenGraphicsAllocationIsCreated) {
    SetUpMm<FamilyType>();
    OsHandleStorage handleStorage;

    auto ptr = (void *)0x1000;
    auto ptr2 = (void *)0x1001;
    auto size = MemoryConstants::pageSize;

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[1].cpuPtr = ptr2;
    handleStorage.fragmentStorageData[2].cpuPtr = nullptr;

    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[1].fragmentSize = size * 2;
    handleStorage.fragmentStorageData[2].fragmentSize = size * 3;

    auto allocation = mm->createGraphicsAllocation(handleStorage, size, ptr);

    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());

    EXPECT_EQ(ptr, allocation->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(ptr2, allocation->fragmentsStorage.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, allocation->fragmentsStorage.fragmentStorageData[2].cpuPtr);

    EXPECT_EQ(size, allocation->fragmentsStorage.fragmentStorageData[0].fragmentSize);
    EXPECT_EQ(size * 2, allocation->fragmentsStorage.fragmentStorageData[1].fragmentSize);
    EXPECT_EQ(size * 3, allocation->fragmentsStorage.fragmentStorageData[2].fragmentSize);

    EXPECT_NE(&allocation->fragmentsStorage, &handleStorage);
    delete allocation;
}

HWTEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsDoesNotMarkAllocationsResidentWhenMakeResidentFails) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    mm->pushAllocationForResidency(&allocation1);
    mm->pushAllocationForResidency(&allocation2);
    mm->pushAllocationForResidency(&allocation3);
    mm->pushAllocationForResidency(&allocation4);

    bool result = mm->makeResidentResidencyAllocations(nullptr);

    EXPECT_FALSE(result);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_FALSE(allocation3.getResidencyData().resident);
    EXPECT_FALSE(allocation4.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsDoesNotMarkTripleAllocationsResidentWhenMakeResidentFails) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2;

    WddmAllocation *allocationTriple = (WddmAllocation *)mm->allocateGraphicsMemory(8196, (void *)0x1500);

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    mm->pushAllocationForResidency(&allocation1);
    mm->pushAllocationForResidency(allocationTriple);
    mm->pushAllocationForResidency(&allocation2);

    bool result = mm->makeResidentResidencyAllocations(nullptr);

    EXPECT_FALSE(result);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident);
    }

    mm->freeGraphicsMemory(allocationTriple);
}

HWTEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsFailsWhenMakeResidentFailsAndCantTrimFurther) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    mm->pushAllocationForResidency(&allocation1);
    mm->pushAllocationForResidency(&allocation2);
    mm->pushAllocationForResidency(&allocation3);
    mm->pushAllocationForResidency(&allocation4);

    bool result = mm->makeResidentResidencyAllocations(nullptr);

    EXPECT_FALSE(result);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_FALSE(allocation3.getResidencyData().resident);
    EXPECT_FALSE(allocation4.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsCallsMakeResidentWithCantTrimFurtherSetToTrueWhenTrimToBudgetReturnsFalse) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1;

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, false, ::testing::_)).Times(1);
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, true, ::testing::_)).Times(1);

    mm->pushAllocationForResidency(&allocation1);

    bool result = mm->makeResidentResidencyAllocations(nullptr);

    EXPECT_FALSE(result);
}

HWTEST_F(WddmMemoryManagerTest2, givenAllocationPackWhenTheyArePassedToMakeResidentThenTheyAreUsedInsteadOfMemoryManagerMembers) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1;
    WddmAllocation allocation2;
    WddmAllocation allocation3;
    allocation1.handle = 1;
    allocation2.handle = 2;
    allocation3.handle = 3;

    ResidencyContainer residencyPack;
    residencyPack.push_back(&allocation1);
    residencyPack.push_back(&allocation2);

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool {
        EXPECT_EQ(1, handles[0]);
        EXPECT_EQ(2, handles[1]);
        return true;
    };
    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, 2, false, ::testing::_)).Times(1);

    mm->pushAllocationForResidency(&allocation3);
    bool result = mm->makeResidentResidencyAllocations(&residencyPack);
    EXPECT_TRUE(result);
}

HWTEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsSucceedsWhenMakeResidentFailsAndTrimToBudgetSucceeds) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1;

    size_t allocationSize = 0x1000;
    WddmAllocation allocationToTrim((void *)0x1000, allocationSize, (void *)0x1000, allocationSize);

    allocationToTrim.getResidencyData().lastFence = wddm->getMonitoredFence().lastSubmittedFence;

    auto makeResidentWithOutBytesToTrim = [allocationSize](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = allocationSize;  return false; };

    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillOnce(::testing::Invoke(makeResidentWithOutBytesToTrim)).WillOnce(::testing::Return(true));

    mm->addToTrimCandidateList(&allocationToTrim);

    mm->pushAllocationForResidency(&allocation1);

    bool result = mm->makeResidentResidencyAllocations(nullptr);

    EXPECT_TRUE(result);

    EXPECT_TRUE(allocation1.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerTest2, givenMemoryManagerWhenMakeResidentFailsThenMemoryBudgetExhaustedIsReturnedAsTrue) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1;

    auto makeResidentThatFails = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { return false; };
    auto makeResidentThatSucceds = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { return true; };

    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillOnce(::testing::Invoke(makeResidentThatFails)).WillOnce(::testing::Invoke(makeResidentThatSucceds));

    mm->pushAllocationForResidency(&allocation1);
    bool result = mm->makeResidentResidencyAllocations(nullptr);
    EXPECT_TRUE(mm->isMemoryBudgetExhausted());
}

TEST(WddmMemoryManagerWithAsyncDeleterTest, givenWddmWhenAsyncDeleterIsEnabledThenCanDeferDeletions) {
    WddmMock *wddm = new WddmMock;
    wddm->callBaseDestroyAllocations = false;
    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    MockWddmMemoryManager memoryManager(wddm);
    memoryManager.setDeferredDeleter(deleter);
    EXPECT_EQ(0, deleter->deferDeletionCalled);
    memoryManager.tryDeferDeletions(nullptr, 0, 0, 0);
    EXPECT_EQ(1, deleter->deferDeletionCalled);
    EXPECT_EQ(1u, wddm->destroyAllocationResult.called);
}

TEST(WddmMemoryManagerWithAsyncDeleterTest, givenWddmWhenAsyncDeleterIsDisabledThenCannotDeferDeletions) {
    WddmMock *wddm = new WddmMock;
    wddm->callBaseDestroyAllocations = false;
    MockWddmMemoryManager memoryManager(wddm);
    memoryManager.setDeferredDeleter(nullptr);
    memoryManager.tryDeferDeletions(nullptr, 0, 0, 0);
    EXPECT_EQ(1u, wddm->destroyAllocationResult.called);
}

TEST(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithAsyncDeleterWhenCannotAllocateMemoryForTiledImageThenDrainIsCalledAndCreateAllocationIsCalledTwice) {
    WddmMock *wddm = new WddmMock;
    wddm->callBaseDestroyAllocations = false;
    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    MockWddmMemoryManager memoryManager(wddm);
    memoryManager.setDeferredDeleter(deleter);

    cl_image_desc imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    ImageInfo imgInfo;
    imgInfo.imgDesc = &imgDesc;
    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    deleter->expectDrainBlockingValue(true);
    memoryManager.allocateGraphicsMemoryForImage(imgInfo, nullptr);
    EXPECT_EQ(1, deleter->drainCalled);
    EXPECT_EQ(2u, wddm->createAllocationResult.called);
}

TEST(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithAsyncDeleterWhenCanAllocateMemoryForTiledImageThenDrainIsNotCalledAndCreateAllocationIsCalledOnce) {
    WddmMock *wddm = new WddmMock;
    wddm->callBaseDestroyAllocations = false;
    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    MockWddmMemoryManager memoryManager(wddm);
    memoryManager.setDeferredDeleter(deleter);

    cl_image_desc imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    ImageInfo imgInfo;
    imgInfo.imgDesc = &imgDesc;
    wddm->createAllocationStatus = STATUS_SUCCESS;
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    auto allocation = memoryManager.allocateGraphicsMemoryForImage(imgInfo, nullptr);
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithoutAsyncDeleterWhenCannotAllocateMemoryForTiledImageThenCreateAllocationIsCalledOnce) {
    WddmMock *wddm = new WddmMock;
    wddm->callBaseDestroyAllocations = false;
    MockWddmMemoryManager memoryManager(wddm);
    memoryManager.setDeferredDeleter(nullptr);

    cl_image_desc imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    ImageInfo imgInfo;
    imgInfo.imgDesc = &imgDesc;
    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    memoryManager.allocateGraphicsMemoryForImage(imgInfo, nullptr);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
}

HWTEST_F(MockWddmMemoryManagerTest, givenValidateAllocationFunctionWhenItIsCalledWithTripleAllocationThenSuccessIsReturned) {
    WddmMock *wddm = new WddmMock;
    EXPECT_TRUE(wddm->init<FamilyType>());
    MockWddmMemoryManager memoryManager(wddm);

    auto wddmAlloc = (WddmAllocation *)memoryManager.allocateGraphicsMemory(4096u, (void *)0x1000);

    EXPECT_TRUE(memoryManager.validateAllocationMock(wddmAlloc));

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

HWTEST_F(MockWddmMemoryManagerTest, givenEnabled64kbpagesWhencreateGraphicsAllocationWithRequiredBitnessThenAllocated64kbAdress) {
    DebugManagerStateRestore dbgRestore;
    WddmMock *wddm = new WddmMock;
    EXPECT_TRUE(wddm->init<FamilyType>());
    DebugManager.flags.Enable64kbpages.set(true);
    WddmMemoryManager memoryManager64k(true, wddm);
    EXPECT_EQ(0, wddm->createAllocationResult.called);
    GraphicsAllocation *galloc = memoryManager64k.createGraphicsAllocationWithRequiredBitness(64 * 1024, nullptr, false);
    EXPECT_EQ(1, wddm->createAllocationResult.called);
    EXPECT_NE(nullptr, galloc);
    EXPECT_EQ(true, galloc->isLocked());
    EXPECT_NE(nullptr, galloc->getUnderlyingBuffer());
    EXPECT_EQ(0u, (uintptr_t)galloc->getUnderlyingBuffer() % 65536U);
    memoryManager64k.freeGraphicsMemory(galloc);
}

HWTEST_F(MockWddmMemoryManagerTest, givenEnabled64kbpagesWhenSetLockThenLockIsSet) {
    DebugManagerStateRestore dbgRestore;
    WddmMock *wddm = new WddmMock;
    EXPECT_TRUE(wddm->init<FamilyType>());
    DebugManager.flags.Enable64kbpages.set(true);
    WddmMemoryManager memoryManager64k(true, wddm);
    EXPECT_EQ(0, wddm->createAllocationResult.called);
    GraphicsAllocation *galloc = memoryManager64k.createGraphicsAllocationWithRequiredBitness(64 * 1024, nullptr, false);
    galloc->setLocked(false);
    EXPECT_FALSE(galloc->isLocked());
    galloc->setLocked(true);
    EXPECT_TRUE(galloc->isLocked());
    memoryManager64k.freeGraphicsMemory(galloc);
}

HWTEST_F(OsAgnosticMemoryManagerUsingWddmTest, GivenEnabled64kbPagesWhenAllocationIsCreatedWithSizeSmallerThen64KBThenGraphicsAllocationsHas64KBAlignedUnderlyingsize) {
    DebugManagerStateRestore dbgRestore;
    WddmMock *wddm = new WddmMock;
    EXPECT_TRUE(wddm->init<FamilyType>());
    DebugManager.flags.Enable64kbpages.set(true);
    WddmMemoryManager memoryManager(true, wddm);
    auto graphicsAllocation = memoryManager.createGraphicsAllocationWithRequiredBitness(1, nullptr);
    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(64 * MemoryConstants::kiloByte, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_NE(0llu, graphicsAllocation->getGpuAddress());
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ((void *)graphicsAllocation->getGpuAddress(), graphicsAllocation->getUnderlyingBuffer());

    EXPECT_EQ(1u, graphicsAllocation->gmm->resourceParams.Flags.Info.Cacheable);

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(MockWddmMemoryManagerTest, givenWddmWhenallocateGraphicsMemory64kbThenLockResultAndmapGpuVirtualAddressIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.Enable64kbpages.set(true);
    WddmMock *wddm = new WddmMock;
    EXPECT_TRUE(wddm->init<FamilyType>());
    MockWddmMemoryManager memoryManager64k(wddm);
    uint32_t lockCount = wddm->lockResult.called;
    uint32_t mapGpuVirtualAddressResult = wddm->mapGpuVirtualAddressResult.called;
    GraphicsAllocation *galloc = memoryManager64k.allocateGraphicsMemory64kb(65536, 65536, true);
    EXPECT_EQ(lockCount + 1, wddm->lockResult.called);
    EXPECT_EQ(mapGpuVirtualAddressResult + 1, wddm->mapGpuVirtualAddressResult.called);
    EXPECT_NE(wddm->mapGpuVirtualAddressResult.cpuPtrPassed, nullptr);
    memoryManager64k.freeGraphicsMemory(galloc);
}

TEST_F(MockWddmMemoryManagerTest, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsTrue) {
    WddmMock *wddm = new WddmMock;
    WddmMemoryManager memoryManager(false, wddm);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
}

TEST_F(MockWddmMemoryManagerTest, givenDefaultWddmMemoryManagerWhenItIsCreatedThenMemoryBudgetIsNotExhausted) {
    WddmMock *wddm = new WddmMock;
    WddmMemoryManager memoryManager(false, wddm);
    EXPECT_FALSE(memoryManager.isMemoryBudgetExhausted());
}

TEST_F(MockWddmMemoryManagerTest, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsTrueAndDeleterIsNotNullptr) {
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(true);
    WddmMock *wddm = new WddmMock;
    WddmMemoryManager memoryManager(false, wddm);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST_F(MockWddmMemoryManagerTest, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(false);
    WddmMock *wddm = new WddmMock;
    WddmMemoryManager memoryManager(false, wddm);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

HWTEST_F(MockWddmMemoryManagerTest, givenPageTableManagerWhenMapAuxGpuVaCalledThenUseWddmToMap) {
    auto myWddm = new WddmMock();
    EXPECT_TRUE(myWddm->init<FamilyType>());
    WddmMemoryManager memoryManager(false, myWddm);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    myWddm->resetPageTableManager(mockMngr);

    auto allocation = memoryManager.allocateGraphicsMemory(4096);

    GMM_DDI_UPDATEAUXTABLE givenDdiUpdateAuxTable = {};
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = allocation->getGpuAddress();
    expectedDdiUpdateAuxTable.BaseResInfo = allocation->gmm->gmmResourceInfo->peekHandle();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(1).WillOnce(Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {givenDdiUpdateAuxTable = *arg; return GMM_SUCCESS; }));

    auto result = memoryManager.mapAuxGpuVA(allocation);
    EXPECT_TRUE(result);
    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &givenDdiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_F(MockWddmMemoryManagerTest, givenRenderCompressedAllocationWhenMappedGpuVaThenMapAuxVa) {
    std::unique_ptr<Gmm> gmm(Gmm::create((void *)123, 4096u, false));
    gmm->isRenderCompressed = true;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm;
    EXPECT_TRUE(wddm.init<FamilyType>());

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm.resetPageTableManager(mockMngr);

    GMM_DDI_UPDATEAUXTABLE givenDdiUpdateAuxTable = {};
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = Gmm::canonize(wddm.getAdapterInfo()->GfxPartition.Standard.Base);
    expectedDdiUpdateAuxTable.BaseResInfo = gmm->gmmResourceInfo->peekHandle();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(1).WillOnce(Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {givenDdiUpdateAuxTable = *arg; return GMM_SUCCESS; }));

    auto result = wddm.mapGpuVirtualAddressImpl(gmm.get(), ALLOCATION_HANDLE, nullptr, 3, gpuVa, false, false);
    ASSERT_TRUE(result);
    EXPECT_EQ(Gmm::canonize(wddm.getAdapterInfo()->GfxPartition.Standard.Base), gpuVa);

    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &givenDdiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
}

HWTEST_F(MockWddmMemoryManagerTest, givenRenderCompressedAllocationWhenReleaseingThenUnmapAuxVa) {
    WddmMock *wddm = new WddmMock();
    EXPECT_TRUE(wddm->init<FamilyType>());
    WddmMemoryManager memoryManager(false, wddm);
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 123;

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm->resetPageTableManager(mockMngr);

    auto wddmAlloc = (WddmAllocation *)memoryManager.allocateGraphicsMemory(4096u, 4096u);
    wddmAlloc->gpuPtr = gpuVa;
    wddmAlloc->gmm->isRenderCompressed = true;

    GMM_DDI_UPDATEAUXTABLE givenDdiUpdateAuxTable = {};
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = gpuVa;
    expectedDdiUpdateAuxTable.BaseResInfo = wddmAlloc->gmm->gmmResourceInfo->peekHandle();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = false;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(1).WillOnce(Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {givenDdiUpdateAuxTable = *arg; return GMM_SUCCESS; }));
    memoryManager.freeGraphicsMemory(wddmAlloc);

    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &givenDdiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
}

HWTEST_F(MockWddmMemoryManagerTest, givenNonRenderCompressedAllocationWhenReleaseingThenDontUnmapAuxVa) {
    WddmMock *wddm = new WddmMock();
    EXPECT_TRUE(wddm->init<FamilyType>());
    WddmMemoryManager memoryManager(false, wddm);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm->resetPageTableManager(mockMngr);

    auto wddmAlloc = (WddmAllocation *)memoryManager.allocateGraphicsMemory(4096u, 4096u);
    wddmAlloc->gmm->isRenderCompressed = false;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

HWTEST_F(MockWddmMemoryManagerTest, givenNonRenderCompressedAllocationWhenMappedGpuVaThenDontMapAuxVa) {
    std::unique_ptr<Gmm> gmm(Gmm::create((void *)123, 4096u, false));
    gmm->isRenderCompressed = false;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm;
    EXPECT_TRUE(wddm.init<FamilyType>());

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm.resetPageTableManager(mockMngr);

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    auto result = wddm.mapGpuVirtualAddressImpl(gmm.get(), ALLOCATION_HANDLE, nullptr, 3, gpuVa, false, false);
    ASSERT_TRUE(result);
}

HWTEST_F(MockWddmMemoryManagerTest, givenFailingAllocationWhenMappedGpuVaThenReturnFalse) {
    std::unique_ptr<Gmm> gmm(Gmm::create((void *)123, 4096u, false));
    gmm->isRenderCompressed = false;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm;
    EXPECT_TRUE(wddm.init<FamilyType>());

    auto result = wddm.mapGpuVirtualAddressImpl(gmm.get(), 0, nullptr, 3, gpuVa, false, false);
    ASSERT_FALSE(result);
}

HWTEST_F(MockWddmMemoryManagerTest, givenRenderCompressedFlagSetWhenInternalIsUnsetThenDontUpdateAuxTable) {
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock *wddm = new WddmMock();
    EXPECT_TRUE(wddm->init<FamilyType>());
    WddmMemoryManager memoryManager(false, wddm);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm->resetPageTableManager(mockMngr);

    auto myGmm = Gmm::create((void *)123, 4096u, false);
    myGmm->isRenderCompressed = false;
    myGmm->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = 1;

    auto wddmAlloc = (WddmAllocation *)memoryManager.allocateGraphicsMemory(4096u, 4096u);
    delete wddmAlloc->gmm;
    wddmAlloc->gmm = myGmm;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    auto result = wddm->mapGpuVirtualAddressImpl(myGmm, ALLOCATION_HANDLE, nullptr, 3, gpuVa, false, false);
    EXPECT_TRUE(result);
    memoryManager.freeGraphicsMemory(wddmAlloc);
}
