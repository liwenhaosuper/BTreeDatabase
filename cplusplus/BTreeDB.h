

#if !defined(__btreedb_h_)
#define __btreedb_h_

#include "DbObj.h"
#include "TreeNode.h"
#include"stdafx.h"

namespace Database
{
	class BTreeDB : public Database::RefCount
	{
	public:
		typedef bool (*traverseCallback)(const DbObjPtr&, const DbObjPtr&, int depth);
		enum ESeqPos
		{
			ESP_START = 0,	// start iterating through the entire tree
			ESP_KEY,		// start from the key provided
			ESP_CONT	// continue from the last position
		};
		enum ESeqDirection
		{
			ESD_FORWARD = 0,	// iterate forwards through the tree
			ESD_BACKWARD		// seek backwards through the tree
		};

	public:
		BTreeDB(const std::string& fileName, size_t recSize = -1, size_t keySize = -1, size_t minDegree = 2, compareFn cfn = 0);
		~BTreeDB(void);

	private:
		size_t _recSize;		// size of the records to be stored
		size_t _keySize;		// number of bytes that comprise the key
		std::string _fileName;		// name of the database file
		compareFn _compFunc;
		size_t _minDegree;
		TreeNodePtr _root;
		FILE* _dataFile;
		size_t _nodeSize;

	private:
		struct SFileHeader
		{
			long rootPos;
			size_t recSize;
			size_t keySize;
			size_t minDegree;
		};

	private:	// internal data manipulation functions (see Cormen, Leiserson, Rivest).
		static int _defaultCompare(const DbObjPtr& obj1, const DbObjPtr& obj2);
		static int _searchCompare(const DbObjPtr& obj1, const DbObjPtr& obj2);
		static bool _searchCallback(const DbObjPtr& obj, const DbObjPtr& ref, int depth);
		TreeNodePtr _allocateNode();
		void _split(TreeNodePtr& parent, size_t childNum, TreeNodePtr& child);
		TreeNodePtr _merge(TreeNodePtr& parent, size_t objNo);
		void _insert(const DbObjPtr& key);
		void _insertNonFull(TreeNodePtr& node, const DbObjPtr& key);
		void _traverse(const TreeNodePtr& node, const DbObjPtr& ref, traverseCallback cbfn, int depth=0);
		NodeKeyLocn _search(const TreeNodePtr& node, const DbObjPtr& key, compareFn cfn = 0);
		bool _seqNext(NodeKeyLocn& locn, DbObjPtr& rec);
		bool _seqPrev(NodeKeyLocn& locn, DbObjPtr& rec);
		bool _flush(TreeNodePtr& node, FILE* f);
		bool _delete(TreeNodePtr& node, const DbObjPtr& key);
		NodeKeyLocn _findPred(TreeNodePtr& node);
		NodeKeyLocn _findSucc(TreeNodePtr& node);

		
	public:
		void close();

		bool open();
		bool del(const DbObjPtr& key);
		bool put(const DbObjPtr& rec);
		bool get(const NodeKeyLocn& locn, DbObjPtr& rec);
		bool get(const DbObjPtr& key, DbObjPtr& rec);
		void traverse(const DbObjPtr& ref = 0, traverseCallback cbfn = 0);
		void findAll(const DbObjPtr& key, DBOBJVECTOR& results);
		NodeKeyLocn search(const DbObjPtr& key, compareFn cfn = 0);
		bool seq(NodeKeyLocn& locn, DbObjPtr& rec, ESeqDirection sdir = ESD_FORWARD);
		bool flush();

		size_t getRecSize() const { return _recSize; }
		size_t getKeySize() const { return _keySize; }
		std::string getFileName() const { return _fileName; }
	};
	typedef Database::Ptr<BTreeDB> BTreeDBPtr;
};

#endif
