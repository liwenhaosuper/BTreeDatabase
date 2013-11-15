

#if !defined(__treenode_h)
#define __treenode_h

#include "DbObj.h"

namespace Database
{
	// Function type used for object comparison callbacks.
	typedef int (*compareFn)(const DbObjPtr&, const DbObjPtr&);

	// Where (from a given child) does the key lay?
	enum EChildPos
	{
		ECP_INTHIS,
		ECP_INLEFT,
		ECP_INRIGHT,
		ECP_NONE
	};
	typedef std::pair<size_t, EChildPos> OBJECTPOS;

	// A BTreeDB is made up of a collection of nodes. A node
	// contains information about which of its parent's children
	// is, how many objects it has, whether or not it's a leaf,
	// where it lives on the disk, whether or not it's actually
	// been loaded from the disk, a collection of records,
	// and (if it's not a leaf) a collection of children.
	// It also contains a ptr to its parent.
	class TreeNode : public Database::RefCount
	{
	public:
		TreeNode();
		~TreeNode();
		Database::Ptr<TreeNode> loadChild(size_t childNo, FILE* f, size_t recSize);
		void unload();
		void unloadChildren();
		bool read(FILE* datafile, size_t recSize);  
		bool write(FILE* f);
		bool delFromLeaf(size_t objNo);
		OBJECTPOS findPos(const DbObjPtr& key, compareFn cfn);

		// This count is the number of objects in the node, rather than the
		// number of children in the node. It should only be used when splitting
		// or joining nodes.
		void setCount(size_t newSize)
		{
			objCount = newSize;
			objects.resize(newSize);
			children.resize(newSize + 1);
		}

	public:
		size_t childNo;
		size_t objCount;
		bool isLeaf;
		bool loaded;
		long fpos;
		DBOBJVECTOR objects;
		std::vector<Database::Ptr<TreeNode> > children;
		Database::Ptr<TreeNode> parent;
	};

	typedef Database::Ptr<TreeNode> TreeNodePtr;
	typedef std::vector<TreeNodePtr> TREENODEVECTOR;
	typedef std::pair<TreeNodePtr, size_t> NodeKeyLocn;
}

#endif
