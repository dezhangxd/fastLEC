#pragma once

#include <memory>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>


extern "C"
{
#include "../deps/cudd/config.h"
#include "../deps/cudd/cudd/cudd.h"
}

namespace fastLEC
{
    /**
     * RAII wrapper for CUDD DdManager
     * Automatically manages the lifecycle of DdManager
     */
    class CuddManager
    {
    private:
        DdManager* manager_;
        
    public:
        CuddManager(int numVars = 0, int numVarsZ = 0, 
                   unsigned int unique = CUDD_UNIQUE_SLOTS,
                   unsigned int cache = CUDD_CACHE_SLOTS,
                   unsigned long maxMemory = 0)
        {
            manager_ = Cudd_Init(numVars, numVarsZ, unique, cache, maxMemory);
            if (!manager_) {
                throw std::runtime_error("Failed to initialize CUDD manager");
            }
        }
        
        ~CuddManager()
        {
            if (manager_) {
                Cudd_Quit(manager_);
            }
        }
        
        // Non-copyable but movable
        CuddManager(const CuddManager&) = delete;
        CuddManager& operator=(const CuddManager&) = delete;
        
        CuddManager(CuddManager&& other) noexcept : manager_(other.manager_)
        {
            other.manager_ = nullptr;
        }
        
        CuddManager& operator=(CuddManager&& other) noexcept
        {
            if (this != &other) {
                if (manager_) {
                    Cudd_Quit(manager_);
                }
                manager_ = other.manager_;
                other.manager_ = nullptr;
            }
            return *this;
        }
        
        DdManager* get() const { return manager_; }
        DdManager* operator->() const { return manager_; }
        
        // Wrapper methods for common CUDD operations
        DdNode* bddNewVar() { return Cudd_bddNewVar(manager_); }
        DdNode* bddAnd(DdNode* f, DdNode* g) { return Cudd_bddAnd(manager_, f, g); }
        DdNode* bddXor(DdNode* f, DdNode* g) { return Cudd_bddXor(manager_, f, g); }
        DdNode* bddOr(DdNode* f, DdNode* g) { return Cudd_bddOr(manager_, f, g); }
        DdNode* bddNot(DdNode* f) { return Cudd_Not(f); }
        
        DdNode* readLogicZero() { return Cudd_ReadLogicZero(manager_); }
        DdNode* readLogicOne() { return Cudd_ReadOne(manager_); }
        
        void ref(DdNode* node) { Cudd_Ref(node); }
        void recursiveDeref(DdNode* node) { Cudd_RecursiveDeref(manager_, node); }
        
        long readNodeCount() { return Cudd_ReadNodeCount(manager_); }
        int readSize() { return Cudd_ReadSize(manager_); }
        int readReorderings() { return Cudd_ReadReorderings(manager_); }
        long readMemoryInUse() { return Cudd_ReadMemoryInUse(manager_); }
        
        // Timeout management
        void setTimeLimit(unsigned long timeLimit) { Cudd_SetTimeLimit(manager_, timeLimit); }
        void setStartTime(unsigned long startTime) { Cudd_SetStartTime(manager_, startTime); }
        void resetStartTime() { Cudd_ResetStartTime(manager_); }
        unsigned long readTimeLimit() { return Cudd_ReadTimeLimit(manager_); }
        unsigned long readElapsedTime() { return Cudd_ReadElapsedTime(manager_); }
        unsigned long readStartTime() { return Cudd_ReadStartTime(manager_); }
        
        // Error checking
        int readErrorCode() { return Cudd_ReadErrorCode(manager_); }
        void clearErrorCode() { Cudd_ClearErrorCode(manager_); }
        bool hasTimeout() { return readErrorCode() == CUDD_TIMEOUT_EXPIRED; }
        bool hasTermination() { return readErrorCode() == CUDD_TERMINATION; }
        bool hasTooManyNodes() { return readErrorCode() == CUDD_TOO_MANY_NODES; }
        
        void printDebug(DdNode* dd, int n, int pr) { Cudd_PrintDebug(manager_, dd, n, pr); }
        void dumpDot(int n, DdNode** dd, char** inames, char** onames, FILE* fp) 
        { 
            Cudd_DumpDot(manager_, n, dd, inames, onames, fp); 
        }
    };
    
    /**
     * RAII wrapper for CUDD DdNode with reference counting
     * Automatically manages the lifecycle of DdNode
     */
    class BDD
    {
    private:
        DdNode* node_;
        std::shared_ptr<CuddManager> manager_;
        
        // Custom deleter for DdNode
        struct DdNodeDeleter
        {
            std::shared_ptr<CuddManager> manager;
            
            DdNodeDeleter(std::shared_ptr<CuddManager> mgr) : manager(mgr) {}
            
            void operator()(DdNode* node) const
            {
                if (node && manager && manager->get()) {
                    Cudd_RecursiveDeref(manager->get(), node);
                }
            }
        };
        
    public:
        BDD() : node_(nullptr), manager_(nullptr) {}
        
        BDD(DdNode* node, std::shared_ptr<CuddManager> manager) 
            : node_(node), manager_(manager)
        {
            if (node_ && manager_) {
                Cudd_Ref(node_);
            }
        }
        
        // Copy constructor
        BDD(const BDD& other) : node_(other.node_), manager_(other.manager_)
        {
            if (node_ && manager_) {
                Cudd_Ref(node_);
            }
        }
        
        // Move constructor
        BDD(BDD&& other) noexcept : node_(other.node_), manager_(other.manager_)
        {
            other.node_ = nullptr;
            other.manager_ = nullptr;
        }
        
        // Assignment operator
        BDD& operator=(const BDD& other)
        {
            if (this != &other) {
                // Dereference current node
                if (node_ && manager_) {
                    Cudd_RecursiveDeref(manager_->get(), node_);
                }
                
                node_ = other.node_;
                manager_ = other.manager_;
                
                // Reference new node
                if (node_ && manager_) {
                    Cudd_Ref(node_);
                }
            }
            return *this;
        }
        
        // Move assignment operator
        BDD& operator=(BDD&& other) noexcept
        {
            if (this != &other) {
                // Dereference current node
                if (node_ && manager_) {
                    Cudd_RecursiveDeref(manager_->get(), node_);
                }
                
                node_ = other.node_;
                manager_ = other.manager_;
                
                other.node_ = nullptr;
                other.manager_ = nullptr;
            }
            return *this;
        }
        
        ~BDD()
        {
            if (node_ && manager_) {
                Cudd_RecursiveDeref(manager_->get(), node_);
            }
        }
        
        // Access methods
        DdNode* get() const { return node_; }
        DdNode* operator->() const { return node_; }
        
        // Boolean operations
        BDD operator&(const BDD& other) const
        {
            if (!manager_ || !other.manager_ || manager_ != other.manager_) {
                throw std::runtime_error("BDD operations require same manager");
            }
            DdNode* result = manager_->bddAnd(node_, other.node_);
            if (result == nullptr) {
                // Check for timeout or other errors - return empty BDD instead of throwing
                if (manager_->hasTimeout() || manager_->hasTooManyNodes()) {
                    return BDD(); // Return empty BDD to indicate timeout/error
                } else {
                    throw std::runtime_error("BDD operation failed");
                }
            }
            return BDD(result, manager_);
        }
        
        BDD operator^(const BDD& other) const
        {
            if (!manager_ || !other.manager_ || manager_ != other.manager_) {
                throw std::runtime_error("BDD operations require same manager");
            }
            DdNode* result = manager_->bddXor(node_, other.node_);
            if (result == nullptr) {
                // Check for timeout or other errors - return empty BDD instead of throwing
                if (manager_->hasTimeout() || manager_->hasTooManyNodes()) {
                    return BDD(); // Return empty BDD to indicate timeout/error
                } else {
                    throw std::runtime_error("BDD operation failed");
                }
            }
            return BDD(result, manager_);
        }
        
        BDD operator|(const BDD& other) const
        {
            if (!manager_ || !other.manager_ || manager_ != other.manager_) {
                throw std::runtime_error("BDD operations require same manager");
            }
            DdNode* result = manager_->bddOr(node_, other.node_);
            if (result == nullptr) {
                // Check for timeout or other errors - return empty BDD instead of throwing
                if (manager_->hasTimeout() || manager_->hasTooManyNodes()) {
                    return BDD(); // Return empty BDD to indicate timeout/error
                } else {
                    throw std::runtime_error("BDD operation failed");
                }
            }
            return BDD(result, manager_);
        }
        
        BDD operator!() const
        {
            if (!manager_) {
                throw std::runtime_error("BDD operations require manager");
            }
            DdNode* result = manager_->bddNot(node_);
            if (result == nullptr) {
                // Check for timeout or other errors - return empty BDD instead of throwing
                if (manager_->hasTimeout() || manager_->hasTooManyNodes()) {
                    return BDD(); // Return empty BDD to indicate timeout/error
                } else {
                    throw std::runtime_error("BDD operation failed");
                }
            }
            return BDD(result, manager_);
        }
        
        // Comparison operators
        bool operator==(const BDD& other) const
        {
            return node_ == other.node_;
        }
        
        bool operator!=(const BDD& other) const
        {
            return node_ != other.node_;
        }
        
        // Utility methods
        bool isNull() const { return node_ == nullptr; }
        bool isZero() const { return manager_ && node_ == manager_->readLogicZero(); }
        bool isOne() const { return manager_ && node_ == manager_->readLogicOne(); }
        
        std::shared_ptr<CuddManager> getManager() const { return manager_; }
    };
    
    /**
     * Factory functions for creating BDDs
     */
    class BDDFactory
    {
    public:
        static BDD createZero(std::shared_ptr<CuddManager> manager)
        {
            return BDD(manager->readLogicZero(), manager);
        }
        
        static BDD createOne(std::shared_ptr<CuddManager> manager)
        {
            return BDD(manager->readLogicOne(), manager);
        }
        
        static BDD createVar(std::shared_ptr<CuddManager> manager)
        {
            DdNode* result = manager->bddNewVar();
            if (result == nullptr) {
                // Check for timeout or other errors - return empty BDD instead of throwing
                if (manager->hasTimeout() || manager->hasTooManyNodes()) {
                    return BDD(); // Return empty BDD to indicate timeout/error
                } else {
                    throw std::runtime_error("BDD variable creation failed");
                }
            }
            return BDD(result, manager);
        }
    };
    
    /**
     * Utility functions for BDD operations
     */
    class BDDUtils
    {
    public:
        static void printDD(std::shared_ptr<CuddManager> manager, const BDD& bdd, int n, int pr)
        {
            printf("DdManager nodes: %ld | ", manager->readNodeCount());
            printf("DdManager vars: %d | ", manager->readSize());
            printf("DdManager reorderings: %d | ", manager->readReorderings());
            printf("DdManager memory: %ld \n", manager->readMemoryInUse());
            manager->printDebug(bdd.get(), n, pr);
        }
        
        static void writeDD(std::shared_ptr<CuddManager> manager, const BDD& bdd, const char* filename)
        {
            char dir_path[256];
            strcpy(dir_path, filename);
            char* last_slash = strrchr(dir_path, '/');
            if (last_slash != NULL) {
                *last_slash = '\0';
#ifdef _WIN32
                _mkdir(dir_path);
#else
                mkdir(dir_path, 0755);
#endif
            }
            
            FILE* outfile = fopen(filename, "w");
            if (!outfile) {
                throw std::runtime_error("Failed to open file for writing");
            }
            
            DdNode** ddnodearray = (DdNode**)malloc(sizeof(DdNode*));
            ddnodearray[0] = bdd.get();
            manager->dumpDot(1, ddnodearray, NULL, NULL, outfile);
            free(ddnodearray);
            fclose(outfile);
        }
    };
}
