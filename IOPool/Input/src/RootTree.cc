#include "RootTree.h"
#include "RootDelayedReader.h"
#include "FWCore/Utilities/interface/EDMException.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "DataFormats/Provenance/interface/BranchDescription.h"
#include "InputFile.h"
#include "TTree.h"
#include "TTreeIndex.h"
#include "TTreeCache.h"

#include <iostream>

namespace edm {
  namespace {
    TBranch* getAuxiliaryBranch(TTree* tree, BranchType const& branchType) {
      TBranch* branch = tree->GetBranch(BranchTypeToAuxiliaryBranchName(branchType).c_str());
      if (branch == 0) {
        branch = tree->GetBranch(BranchTypeToAuxBranchName(branchType).c_str());
      }
      return branch;
    }
    TBranch* getProductProvenanceBranch(TTree* tree, BranchType const& branchType) {
      TBranch* branch = tree->GetBranch(BranchTypeToBranchEntryInfoBranchName(branchType).c_str());
      return branch;
    }
  }
  RootTree::RootTree(boost::shared_ptr<InputFile> filePtr,
                     BranchType const& branchType,
                     unsigned int maxVirtualSize,
                     unsigned int cacheSize,
                     unsigned int learningEntries) :
    filePtr_(filePtr),
    tree_(dynamic_cast<TTree*>(filePtr_.get() != 0 ? filePtr_->Get(BranchTypeToProductTreeName(branchType).c_str()) : 0)),
    metaTree_(dynamic_cast<TTree*>(filePtr_.get() != 0 ? filePtr_->Get(BranchTypeToMetaDataTreeName(branchType).c_str()) : 0)),
    branchType_(branchType),
    auxBranch_(tree_ ? getAuxiliaryBranch(tree_, branchType_) : 0),
    treeCache_(),
    rawTreeCache_(),
    entries_(tree_ ? tree_->GetEntries() : 0),
    entryNumber_(-1),
    branchNames_(),
    branches_(new BranchMap),
    trainNow_(false),
    switchOverEntry_(-1),
    learningEntries_(learningEntries),
    cacheSize_(cacheSize),
    treeAutoFlush_(tree_ ? tree_->GetAutoFlush() : 0),
    rootDelayedReader_(new RootDelayedReader(*this, filePtr)),
    branchEntryInfoBranch_(metaTree_ ? getProductProvenanceBranch(metaTree_, branchType_) : (tree_ ? getProductProvenanceBranch(tree_, branchType_) : 0)),
    infoTree_(dynamic_cast<TTree*>(filePtr_.get() != 0 ? filePtr->Get(BranchTypeToInfoTreeName(branchType).c_str()) : 0)) // backward compatibility
    {
      assert(tree_);
      // On merged files in older releases of ROOT, the autoFlush setting is always negative; we must guess.
      // TODO: On newer merged files, we should be able to get this from the cluster iterator.
      if (treeAutoFlush_ < 0) {
        // The "+1" is here to avoid divide-by-zero in degenerate cases.
        Long64_t averageEventSizeBytes = tree_->GetZipBytes() / (tree_->GetEntries()+1) + 1;
        treeAutoFlush_ = cacheSize_/averageEventSizeBytes+1;
      }
      setTreeMaxVirtualSize(maxVirtualSize);
      setCacheSize(cacheSize);
  }

  RootTree::~RootTree() {
  }

  bool
  RootTree::isValid() const {
    if (metaTree_ == 0 || metaTree_->GetNbranches() == 0) {
      return tree_ != 0 && auxBranch_ != 0;
    }
    if (tree_ != 0 && auxBranch_ != 0 && metaTree_ != 0) { // backward compatibility
      if (branchEntryInfoBranch_ != 0 || infoTree_ != 0) return true; // backward compatibility
      return (entries_ == metaTree_->GetEntries() && tree_->GetNbranches() <= metaTree_->GetNbranches() + 1);  // backward compatibility
    } // backward compatibility
    return false;
  }

  DelayedReader*
  RootTree::rootDelayedReader() const {
    rootDelayedReader_->reset();
    return rootDelayedReader_.get();
  }  

  void
  RootTree::setPresence(BranchDescription const& prod, std::string const& oldBranchName) {
      assert(isValid());
      prod.init();
      if(tree_->GetBranch(oldBranchName.c_str()) == 0){
        prod.setDropped();
      }
  }

  void
  RootTree::addBranch(BranchKey const& key,
                      BranchDescription const& prod,
                      std::string const& oldBranchName) {
      assert(isValid());
      prod.init();
      //use the translated branch name
      TBranch* branch = tree_->GetBranch(oldBranchName.c_str());
      roottree::BranchInfo info = roottree::BranchInfo(ConstBranchDescription(prod));
      info.productBranch_ = 0;
      if (prod.present()) {
        info.productBranch_ = branch;
        //we want the new branch name for the JobReport
        branchNames_.push_back(prod.branchName());
      }
      TTree* provTree = (metaTree_ != 0 ? metaTree_ : tree_);
      info.provenanceBranch_ = provTree->GetBranch(oldBranchName.c_str());
      branches_->insert(std::make_pair(key, info));
  }

  void
  RootTree::dropBranch(std::string const& oldBranchName) {
      //use the translated branch name
      TBranch* branch = tree_->GetBranch(oldBranchName.c_str());
      if (branch != 0) {
        TObjArray* leaves = tree_->GetListOfLeaves();
        int entries = leaves->GetEntries();
        for (int i = 0; i < entries; ++i) {
          TLeaf* leaf = (TLeaf*)(*leaves)[i];
          if (leaf == 0) continue;
          TBranch* br = leaf->GetBranch();
          if (br == 0) continue;
          if (br->GetMother() == branch) {
            leaves->Remove(leaf);
          }
        }
        leaves->Compress();
        tree_->GetListOfBranches()->Remove(branch);
        tree_->GetListOfBranches()->Compress();
        delete branch;
      }
  }

  roottree::BranchMap const&
  RootTree::branches() const {return *branches_;}

  void
  RootTree::setCacheSize(unsigned int cacheSize) {
    cacheSize_ = cacheSize;
    tree_->SetCacheSize(static_cast<Long64_t>(cacheSize));
    treeCache_.reset(dynamic_cast<TTreeCache*>(filePtr_->GetCacheRead()));
    filePtr_->SetCacheRead(0);
    rawTreeCache_.reset();
  }

  void
  RootTree::setTreeMaxVirtualSize(int treeMaxVirtualSize) {
    if (treeMaxVirtualSize >= 0) tree_->SetMaxVirtualSize(static_cast<Long64_t>(treeMaxVirtualSize));
  }

  void
  RootTree::setEntryNumber(EntryNumber theEntryNumber) {
    filePtr_->SetCacheRead(treeCache_.get());

    // Detect a backward skip.  If the skip is sufficiently large, we roll the dice and reset the treeCache.
    // This will cause some amount of over-reading: we pre-fetch all the events in some prior cluster.
    // However, because reading one event in the cluster is supposed to be equivalent to reading all events in the cluster,
    // we're not incurring additional over-reading - we're just doing it more efficiently.
    // NOTE: Constructor guarantees treeAutoFlush_ is positive, even if TTree->GetAutoFlush() is negative.
    if ((theEntryNumber < static_cast<EntryNumber>(entryNumber_-treeAutoFlush_)) &&
        (treeCache_) && (!treeCache_->IsLearning()) && (entries_ > 0) && (switchOverEntry_ >= 0)) {
      treeCache_->SetEntryRange(theEntryNumber, entries_);
      treeCache_->FillBuffer();
    }

    entryNumber_ = theEntryNumber;
    tree_->LoadTree(entryNumber_);
    filePtr_->SetCacheRead(0);
    if(treeCache_ && trainNow_ && entryNumber_ >= 0) {
      startTraining();
      trainNow_ = false;
    }
    if (treeCache_ && treeCache_->IsLearning() && switchOverEntry_ >= 0 && entryNumber_ >= switchOverEntry_) {
      stopTraining();
    }
  }

  void
  RootTree::getEntry(TBranch* branch, EntryNumber entryNumber) const {
    try {
      if (!treeCache_) {
        filePtr_->SetCacheRead(0);
        branch->GetEntry(entryNumber);
      } else if (treeCache_->IsLearning() && rawTreeCache_) {
        treeCache_->AddBranch(branch, kTRUE);
        filePtr_->SetCacheRead(rawTreeCache_.get());
        branch->GetEntry(entryNumber);
        filePtr_->SetCacheRead(0);
      } else {
        filePtr_->SetCacheRead(treeCache_.get());
        branch->GetEntry(entryNumber);
        filePtr_->SetCacheRead(0);
      }
    } catch(cms::Exception const& e) {
      // We make sure the treeCache_ is detached from the file,
      // so that ROOT does not also delete it.
      filePtr_->SetCacheRead(0);
      Exception t(errors::FileReadError, "", e);
      t.addContext(std::string("Reading branch ")+branch->GetName());
      throw t;
    }
  }

  void
  RootTree::startTraining() {
    if (cacheSize_ == 0) {
      return;
    }
    assert(treeCache_ && treeCache_->GetOwner() == tree_);
    assert(branchType_ == InEvent);
    assert(!rawTreeCache_);
    treeCache_->SetLearnEntries(learningEntries_);
    tree_->SetCacheSize(static_cast<Long64_t>(cacheSize_));
    rawTreeCache_.reset(dynamic_cast<TTreeCache *>(filePtr_->GetCacheRead()));
    filePtr_->SetCacheRead(0);
    rawTreeCache_->SetLearnEntries(0);
    switchOverEntry_ = entryNumber_ + learningEntries_;
    rawTreeCache_->StartLearningPhase();
    rawTreeCache_->SetEntryRange(entryNumber_, switchOverEntry_);
    rawTreeCache_->AddBranch("*", kTRUE);
    rawTreeCache_->StopLearningPhase();
    treeCache_->StartLearningPhase();
    treeCache_->SetEntryRange(switchOverEntry_, tree_->GetEntries());
    treeCache_->AddBranch(poolNames::branchListIndexesBranchName().c_str(), kTRUE);
    treeCache_->AddBranch(BranchTypeToAuxiliaryBranchName(branchType_).c_str(), kTRUE);
  }

  void
  RootTree::stopTraining() {
    filePtr_->SetCacheRead(treeCache_.get());
    treeCache_->StopLearningPhase();
    rawTreeCache_.reset();
  }

  void
  RootTree::close () {
    // The TFile is about to be closed, and destructed.
    // Just to play it safe, zero all pointers to quantities that are owned by the TFile.
    auxBranch_  = branchEntryInfoBranch_ = 0;
    tree_ = metaTree_ = infoTree_ = 0;
    // We own the treeCache_.
    // We make sure the treeCache_ is detached from the file,
    // so that ROOT does not also delete it.
    filePtr_->SetCacheRead(0);
    // We give up our shared ownership of the TFile itself.
    filePtr_.reset();
  }

  void
  RootTree::trainCache(char const* branchNames) {
    if (cacheSize_ == 0) {
      return;
    }
    tree_->LoadTree(0);
    assert(treeCache_);
    filePtr_->SetCacheRead(treeCache_.get());
    assert(treeCache_->GetOwner() == tree_);
    treeCache_->StartLearningPhase();
    treeCache_->SetEntryRange(0, tree_->GetEntries());
    treeCache_->AddBranch(branchNames, kTRUE);
    treeCache_->StopLearningPhase();
    // We own the treeCache_.
    // We make sure the treeCache_ is detached from the file,
    // so that ROOT does not also delete it.
    filePtr_->SetCacheRead(0);
  }

  namespace roottree {
    Int_t
    getEntry(TBranch* branch, EntryNumber entryNumber) {
      Int_t n = 0;
      try {
        n = branch->GetEntry(entryNumber);
      }
      catch(cms::Exception const& e) {
        throw Exception(errors::FileReadError, "", e);
      }
      return n;
    }

    Int_t
    getEntry(TTree* tree, EntryNumber entryNumber) {
      Int_t n = 0;
      try {
        n = tree->GetEntry(entryNumber);
      }
      catch(cms::Exception const& e) {
        throw Exception (errors::FileReadError, "", e);
      }
      return n;
    }

    std::unique_ptr<TTreeCache>
    trainCache(TTree* tree, InputFile& file, unsigned int cacheSize, char const* branchNames) {
      tree->LoadTree(0);
      tree->SetCacheSize(cacheSize);
      std::unique_ptr<TTreeCache> treeCache(dynamic_cast<TTreeCache*>(file.GetCacheRead()));
      if (0 != treeCache.get()) {
        treeCache->StartLearningPhase();
        treeCache->SetEntryRange(0, tree->GetEntries());
        treeCache->AddBranch(branchNames, kTRUE);
        treeCache->StopLearningPhase();
      }
      // We own the treeCache_.
      // We make sure the treeCache_ is detached from the file,
      // so that ROOT does not also delete it.
      file.SetCacheRead(0);
      return treeCache;
    }
  }
}
