

#include "stdafx.h"
#include "treenode.h"

namespace Database
{
	// Constructor initialises everything to its default value.
	// Not that we assume to start with that the node is a leaf,
	// and it is not loaded from the disk.
	TreeNode::TreeNode()
		: childNo((size_t)-1)
		, objCount(0)
		, isLeaf(true)
		, loaded(false)
		, fpos(-1)
	{
	}

	// To delete a TreeNode, we must unload each of its children.
	// This must be done, or we won't end up cleaning up all of
	// the loaded children, thus introducing memory leaks.
	TreeNode::~TreeNode()
	{
		TREENODEVECTOR::iterator tnvit = children.begin();
		while (tnvit != children.end())
		{
			if ((TreeNode*)(*tnvit) != 0)
			{
				(*tnvit)->unload();
			}
			++tnvit;
		}
	}

	// Read a node from the disk
	bool TreeNode::read(FILE* f, size_t recSize)
	{
		// Bug out if we don't have a good file.
		if (!f)
		{
			return false;
		}

		// get to the right location
		if (0 != fseek(f, fpos, SEEK_SET))
		{
			return false;
		}

		// read the leaf flag and the object count
		byte leafFlag = 0;
		if (1 != fread(&leafFlag, sizeof(byte), 1, f))
		{
			return false;
		}
		if (1 != fread(&objCount, sizeof(size_t), 1, f))
		{
			return false;
		}
		isLeaf = (leafFlag == 1);

		// read the contents
		objects.resize(objCount);
		byte* pBuf = new byte[recSize];
		for (size_t ctr = 0; ctr < objCount; ctr++)
		{
			if (1 != fread(pBuf, recSize, 1, f))
			{
				delete[] pBuf;	
				return false;
			}
			DbObjPtr pObj = new DbObj(pBuf, recSize);
			objects[ctr] = pObj;
		}
		delete[] pBuf;	

		// read the addresses of the child pages
		if (objCount > 0)
		{
			long* childAddresses = new long[objCount + 1];
			long* thisChild = childAddresses;
			memset(childAddresses, 0xff, sizeof(long) * (objCount + 1));
			if (objCount + 1 != fread(childAddresses, sizeof(long), objCount + 1, f))
			{
				delete[] childAddresses;	
				return false;
			}
			children.resize(objCount + 1);
			for (size_t ctr = 0; ctr <= objCount; ctr++)
			{
				TreeNodePtr newNode = new TreeNode;
				newNode->fpos = *thisChild++;
				children[ctr] = newNode;
				newNode->childNo = ctr;
			}
			delete[] childAddresses;	
		}
		loaded = true;
		return true;
	}

	// Write a node to the disk
	bool TreeNode::write(FILE* f)
	{
		// If we're not loaded, we haven't been changed,
		// so we can say that the flush was successful.
		if (!loaded)
		{
			printf("not load");
			return true;



		}
	    
		// Can't read without a good file ...
		if (!f)
		{
			return false;
		}

		// get to the right location
		if (0 != fseek(f, fpos, SEEK_SET))
		{
			return false;
		}

		// write the leaf flag and the object count
		byte leafFlag = isLeaf ? 1 : 0;
		if (1 != fwrite(&leafFlag, sizeof(byte), 1, f))
		{
			return false;
		}
		if (1 != fwrite(&objCount, sizeof(size_t), 1, f))
		{
			return false;
		}

		// write the contents
		DBOBJVECTOR::iterator dovit = objects.begin();
		while (dovit != objects.end())
		{
			DbObj* pObj = (DbObj*)(*dovit);
			if (1 != fwrite(pObj->getData(), pObj->getSize(), 1, f))
			{
				return false;
			}
			++dovit;
		}

		// write the addresses of the child pages
		if (objCount > 0 && !isLeaf)
		{
			long* childAddresses = new long[objCount + 1];
			long* thisChild = childAddresses;
			memset(childAddresses, 0xff, sizeof(long) * (objCount + 1));
			TREENODEVECTOR::iterator tnvit = children.begin();
			while (tnvit != children.end())
			{
				if ((TreeNode*)(*tnvit) != 0)
				{
					*thisChild = (*tnvit)->fpos;
				}
				++thisChild;
				++tnvit;
			}
			size_t longsWritten = fwrite(childAddresses, sizeof(long), objCount + 1, f);
			delete[] childAddresses;
			if (objCount + 1 != longsWritten)
			{
				return false;
			}
		}
		return true;
	}

	// Load a child node from the disk. This requires that we
	// have the filepos already in place.
	TreeNodePtr TreeNode::loadChild(size_t childNo, FILE* f, size_t recSize)
	{
		TreeNodePtr child = children[childNo];
		if ((TreeNode*)child == 0)
		{
			child = new TreeNode;
			children[childNo] = child;
		}
		if (!child->loaded)
		{
			child->read(f, recSize);
			child->parent = this;
		}
		return child;
	}

	// Unload a child. This means that we get rid of all
	// children in the children vector.
	void TreeNode::unload()
	{
		if (loaded)
		{
			// Clear out all of the objects
			DBOBJVECTOR::iterator dovit = objects.begin();
			while (dovit != objects.end())
			{
				*dovit = (DbObj*)0;
				++dovit;
			}
			objects.resize(0);

			// Clear out all of the children
			TREENODEVECTOR::iterator tnvit = children.begin();
			while (!isLeaf && tnvit != children.end())
			{
				(*tnvit)->unload();
				*tnvit = (TreeNode*)0;
				++tnvit;
			}
			children.resize(0);

			// Empty the parent node and indicate that the
			// node is no longer loaded.
			parent = (TreeNode*)0;
			loaded = false;
		}
	}

	// Delete a child from a given node.
	bool TreeNode::delFromLeaf(size_t objNo)
	{
		bool ret = isLeaf;
		if (ret)
		{
			objects[objNo] = (DbObj*)0;
			for (size_t ctr = objNo + 1; ctr < objCount; ctr++)
			{
				objects[ctr - 1] = objects[ctr];
			}
			setCount(objCount - 1);
		}
		return ret;
	}
	
	// Find the position of the object in a node. If the key is at pos
	// the function returns (pos, ECP_INTHIS). If the key is in a child to
	// the left of pos, the function returns (pos, ECP_INLEFT). If the node
	// is an internal node, the function returns (objCount, ECP_INRIGHT).
	// Otherwise, the function returns ((size_t)-1, false).
	// The main assumption here is that we won't be searching for a key
	// in this node unless it (a) is not in the tree, or (b) it is in the
	// subtree rooted at this node.
	OBJECTPOS TreeNode::findPos(const DbObjPtr& key, compareFn cfn)
	{
		OBJECTPOS ret((size_t)-1, ECP_NONE);
		DBOBJVECTOR::iterator dovit = objects.begin();
		size_t ctr = 0;
		while (dovit < objects.end())
		{
			int compVal = cfn(key, *dovit);
			if (compVal == 0)
			{
				return OBJECTPOS(ctr, ECP_INTHIS);
			}
			else if (compVal < 0)
			{
				if (isLeaf)
				{
					return ret;
				}
				else
				{
					return OBJECTPOS(ctr, ECP_INLEFT);
				}
			}
			++dovit, ++ctr;
		}
		if (!isLeaf)
		{
			return OBJECTPOS(ctr - 1, ECP_INRIGHT);
		}
		return ret;
	}
}