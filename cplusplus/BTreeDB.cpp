

#include "stdafx.h"
#include "btreedb.h"

using namespace std;

namespace Database
{
	// The constructor simply sets up the different data members, and if
	// the caller doesn't provide a compare function of their own, specifies
	// the default comparison function.
	BTreeDB::BTreeDB(const std::string& fileName, size_t recSize, size_t keySize, size_t minDegree, compareFn cfn)
		: _recSize(recSize)
		, _keySize(keySize)
		, _fileName(fileName)
		, _compFunc(cfn)
		, _minDegree(minDegree)
		, _dataFile(0)
		, _nodeSize((size_t)-1)
	{
		if (!_compFunc)
		{
			_compFunc = _defaultCompare;
		}
	}

	// The destructor of a BTreeDB object unloads the root
	// node and then assigns null to the _root smart pointer.
	// This deletes the item that was already there.
	// Unloading the root node ensures that there are no
	// children floating around referring to the root node, so
	// that when we come to assign null to the smart pointer,
	// the number of references will drop to zero, and the
	// actual node will be deleted.
	// We also have to close the file.
	BTreeDB::~BTreeDB(void)
	{
		_root->unload();
		_root = (TreeNode*)0;
		if (_dataFile != 0)
		{
			fclose(_dataFile);
			_dataFile = 0;
		}
	}

	// This is the static default comparator. It just
	// compares the contents of the two DbObj's being
	// compared using memcmp.
	int BTreeDB::_defaultCompare(const DbObjPtr& obj1, const DbObjPtr& obj2)
	{
		return memcmp(obj1->getData(), obj2->getData(), min(obj1->getSize(), obj2->getSize()));
	}

	// Allocate a new node for this tree. This method
	// allocates space in the file for this node, so
	// only do this when you actually need a new node
	// added to the file.
	TreeNodePtr BTreeDB::_allocateNode()
	{
		TreeNodePtr newNode = new TreeNode;
		int fh = _fileno(_dataFile);
		newNode->fpos = _filelength(fh);
		newNode->loaded = true;
		_chsize(fh, (long)(newNode->fpos + _nodeSize));
		return newNode;
	}

	// Search for a key given a starting node. This method returns a pair containing
	// a reference to the node containing the key, and the offset of the key within
	// the node. If not found, the resulting pair will have a null tree node pointer
	// and a location of -1.
	NodeKeyLocn BTreeDB::_search(const TreeNodePtr& node, const DbObjPtr& key, compareFn cfn)
	{
		NodeKeyLocn ret(TreeNodePtr(), (size_t)-1);
		if (cfn == 0)
		{
			cfn = _compFunc;
		}

		OBJECTPOS op = node->findPos(key, cfn);
		if (op.first != (size_t)-1)
		{
			TreeNodePtr child;
			switch (op.second)
			{
			case ECP_INTHIS:
				// If the key is present in the tested node,
				// the result contains a reference to this
				// node and the position within the node.
				ret.first = node;
				ret.second = op.first;
				break;

			case ECP_INLEFT:
				// If the key is present in a child to the
				// left of the tested node, recurse in to the
				// child on the left.
				child = node->loadChild(op.first, _dataFile, _recSize);
				ret = _search(child, key);
				break;

			case ECP_INRIGHT:
				// If the key is present in a child to the
				// right of the tested node, recurse in to the
				// child on the right.
				child = node->loadChild(op.first + 1, _dataFile, _recSize);
				ret = _search(child, key);
				break;

			default:
				break;
			}
		}
		return ret;
	}

	// Splits a child node, creating a new node. The median value from the
	// full child is moved into the *non-full* parent. The keys above the
	// median are moved from the full child to the new child.
	void BTreeDB::_split(TreeNodePtr& parent, size_t childNum, TreeNodePtr& child)
	{
		size_t ctr = 0;
		DbObjPtr saveObj;
		TreeNodePtr newChild = _allocateNode();
		newChild->isLeaf = child->isLeaf;
		newChild->setCount(_minDegree - 1);

		// Put the high values in the new child, then shrink the existing child.
		for (ctr = 0; ctr < _minDegree - 1; ctr++)
		{
			newChild->objects[ctr] = child->objects[_minDegree + ctr];
		}
		if (!child->isLeaf)
		{
			for (ctr = 0; ctr < _minDegree; ctr++)
			{
				TreeNodePtr mover = child->children[_minDegree + ctr];
				newChild->children[ctr] = mover;
				mover->childNo = ctr;
				mover->parent = newChild;
			}
		}
		saveObj = child->objects[_minDegree - 1];
		child->setCount(_minDegree - 1);

		// Move the child pointers above childNum up in the parent
		parent->setCount(parent->objCount + 1);
		for (ctr = parent->objCount; ctr > childNum + 1; ctr--)
		{
			parent->children[ctr] = parent->children[ctr - 1];
			parent->children[ctr]->childNo = ctr;
		}
		parent->children[childNum + 1] = newChild;
		newChild->childNo = childNum + 1;
		newChild->parent = parent;
		for (ctr = parent->objCount - 1; ctr > childNum; ctr--)
		{
			parent->objects[ctr] = parent->objects[ctr - 1];
		}
		parent->objects[childNum] = saveObj;
//problem?
		child->write(_dataFile);
		newChild->write(_dataFile);
		parent->write(_dataFile);
	}

	// Merges two child nodes, removing one node from the parent and
	// putting it into the new merged node. This is the inverse of the
	// _split method above. Only the object number is given, since the
	// children to be merged can be derived from that.
	// The assumption here is that both c1 and c2 have _minDegree - 1
	// keys.
	TreeNodePtr BTreeDB::_merge(TreeNodePtr& parent, size_t objNo)
	{
		size_t ctr = 0;
		TreeNodePtr c1 = parent->children[objNo];
		TreeNodePtr c2 = parent->children[objNo + 1];

		// Make the two child nodes into a single node
		c1->objects.resize(2 * _minDegree - 1);
		for (ctr = 0; ctr < _minDegree - 1; ctr++)
		{
			c1->objects[_minDegree + ctr] = c2->objects[ctr];
		}
		if (!c2->isLeaf)
		{
			c1->children.resize(2 * _minDegree);
			for (ctr = 0; ctr < _minDegree; ctr++)
			{
				size_t newPos = _minDegree + ctr;
				c1->children[newPos] = c2->children[ctr];
				c1->children[newPos]->childNo = newPos; // Thanks steradrian
			}
		}

		// Put the parent into the middle
		c1->objects[_minDegree - 1] = parent->objects[objNo];
		c1->objCount = 2 * _minDegree - 1;

		// Reshuffle the parent (it has one less object/child)
		for (ctr = objNo + 1; ctr < parent->objCount; ctr++)
		{
			parent->objects[ctr - 1] = parent->objects[ctr];
			parent->children[ctr] = parent->children[ctr + 1];
			parent->children[ctr]->childNo = ctr;
		}
		--parent->objCount;
		parent->objects.resize(parent->objCount);
		parent->children.resize(parent->objCount + 1);

		// Write the two affected nodes to the disk. Note that
		// c2 just goes away. The node will be deallocated because
		// of the smart pointers, and the node's location on
		// disk will become inaccessible. This will have to be
		// fixed by the judicious use of the compact() method.
		c1->write(_dataFile);
		parent->write(_dataFile);

		// Return a pointer to the new child.
		return c1;
	}

	// Insert a new key into the btree. If the root has
	// (2t - 1) keys, the tree is full, and should therefore
	// grow. Otherwise, we're inserting into a node that
	// is not full.
	void BTreeDB::_insert(const DbObjPtr& key)
	{
		if (_root->objCount == (_minDegree * 2) - 1)
		{
			// Growing the tree happens by creating a new
			// node as the new root, and splitting the
			// old root into a pair of children.
			TreeNodePtr oldRoot = _root;
			_root = _allocateNode();
			_root->setCount(0);
			_root->isLeaf = false;
			_root->children[0] = oldRoot;
			oldRoot->childNo = 0;
			oldRoot->parent = _root;
			_split(_root, 0, oldRoot);
			_insertNonFull(_root, key);
			fseek(_dataFile, 0, SEEK_SET);
			fwrite(&_root->fpos, sizeof(_root->fpos), 1, _dataFile);
		}
		else
		{
			_insertNonFull(_root, key);
		}
	}

	// Insert a key into a non-full node.
	void BTreeDB::_insertNonFull(TreeNodePtr& node, const DbObjPtr& key)
	{
		size_t ctr = node->objCount;

		// If the node is a leaf, we just find the location to insert
		// the new item, and shuffle everything else up.
		if (node->isLeaf)
		{
			node->setCount(node->objCount + 1);
			for ( ; ctr > 0; ctr--)
			{
				DbObjPtr compObj = node->objects[ctr - 1];
				int compVal = _compFunc(key, compObj);
				if (compVal < 0)
				{
					node->objects[ctr] = node->objects[ctr - 1];
				}
				else
				{
					break;
				}
			}
			node->objects[ctr] = key;
			node->write(_dataFile);
		}

		// If the node is an internal node, we need to find
		// the location to insert the value ...
		else
		{
			while (ctr > 0)
			{
				--ctr;
				int compVal = _compFunc(key, node->objects[ctr]);
				if (compVal >= 0)
				{
					++ctr;
					break;
				}
			}

			// Load the child into which the value will be inserted.
			TreeNodePtr child = node->loadChild(ctr, _dataFile, _recSize);

			// If the child node is full (2t - 1 objects), then we need
			// to split the node.
			if (child->objCount == _minDegree * 2 - 1)
			{
				_split(node, ctr, child);
				int compVal = _compFunc(key, node->objects[ctr]);
				if (compVal > 0)
				{
					++ctr;
				}
				child = node->children[ctr];
			}

			// Insert the key (recursively) into the non-full child
			// node.
			_insertNonFull(child, key);
		}
	}

	// Perform an in-order traversal of the tree
	void BTreeDB::_traverse(const TreeNodePtr& node, const DbObjPtr& ref, traverseCallback cbfn, int depth)
	{
		bool shouldContinue = true;
		size_t ctr = 0;
		for ( ; ctr < node->objCount && shouldContinue; ctr++)
		{
			if (!node->isLeaf)
			{
				TreeNodePtr child = node->loadChild(ctr, _dataFile, _recSize);
				_traverse(child, ref, cbfn, depth + 1);
			}
			shouldContinue = cbfn ? cbfn(node->objects[ctr], ref, depth) : true;
		}
		if (shouldContinue && !node->isLeaf)
		{
			TreeNodePtr child = node->loadChild(ctr, _dataFile, _recSize);
			_traverse(child, ref, cbfn, depth + 1);
		}
	}

	// Write all nodes in the tree to the file given.
	bool BTreeDB::_flush(TreeNodePtr& node, FILE* f)
	{
		bool ret = false;

		// Bug out if the file is not valid
		if (!f)
		{
			return false;
		}

		// If the node isn't loaded ignore it, but
		// return true because it's unchanged.
		if (!node->loaded)
		{
			ret = true;
		}
		else
		{
			// Write the given node to disk ...
			ret = node->write(f);

			// ... and if it has children, make
			// sure they're written too.
			if (ret && !node->isLeaf)
			{
				TREENODEVECTOR::iterator tnvit = node->children.begin();
				while (ret && tnvit != node->children.end())
				{
					ret = _flush((*tnvit), f);
					++tnvit;
				}
			}
		}
		return ret;
	}

	// Internal delete function, used once we've identified the
	// location of the node from whicha key is to be deleted.
	bool BTreeDB::_delete(TreeNodePtr& node, const DbObjPtr& key)
	{
		bool ret = false;

		// Find the object position. op will have the position
		// of the object in op.first, and a flag (op.second)
		// saying whether the object at op.first is an exact
		// match (true) or if the object is in a child of the
		// current node (false). If op.first is -1, the object
		// is neither in this node, or a child node.
		OBJECTPOS op = node->findPos(key, _compFunc);
		if (op.first != (size_t)-1)	// it's in there somewhere ...
		{
			if (op.second == ECP_INTHIS)	// we've got an exact match
			{
				// Case 1: deletion from leaf node.
				if (node->isLeaf)
				{
					node->delFromLeaf(op.first);
					ret = true;
				}

				// Case 2: Exact match on internal leaf.
				else
				{
					// Case 2a: prior child has enough objects to pull one out.
					if (node->children[op.first]->objCount >= _minDegree)
					{
						TreeNodePtr childNode = node->loadChild(op.first, _dataFile, _recSize);
						NodeKeyLocn locn = _findPred(childNode);
						DbObjPtr childObj = locn.first->objects[locn.second];
						ret = _delete(childNode, childObj);
						node->objects[op.first] = childObj;
					}

					// Case 2b: successor child has enough objects to pull one out.
					else if (node->children[op.first + 1]->objCount >= _minDegree)
					{
						TreeNodePtr childNode = node->loadChild(op.first + 1, _dataFile, _recSize);
						NodeKeyLocn locn = _findSucc(childNode);
						DbObjPtr childObj = locn.first->objects[locn.second];
						ret = _delete(childNode, childObj);
						node->objects[op.first] = childObj;
					}

					// Case 2c: both children have only t-1 objects.
					// Merge the two children, putting the key into the
					// new child. Then delete from the new child.
					else
					{
						TreeNodePtr mergedChild = _merge(node, op.first);
						ret = _delete(mergedChild, key);
					}
				}
			}

			// Case 3: key is not in the internal node being examined,
			// but is in one of the children.
			else if (op.second == ECP_INLEFT || op.second == ECP_INRIGHT)
			{
				// Find out if the child tree containing the key
				// has enough objects. If so, we just recurse into
				// that child.
				size_t keyChildPos = (op.second == ECP_INLEFT) ? op.first : op.first + 1;
				TreeNodePtr childNode = node->loadChild(keyChildPos, _dataFile, _recSize);
				if (childNode->objCount >= _minDegree)
				{
					ret = _delete(childNode, key);
				}
				else
				{
					// Find out if the childNode has an immediate
					// sibling with _minDegree keys.
					TreeNodePtr leftSib;
					TreeNodePtr rightSib;
					size_t leftCount = 0;
					size_t rightCount = 0;
					if (keyChildPos > 0)
					{
						leftSib = node->loadChild(keyChildPos - 1, _dataFile, _recSize);
						leftCount = leftSib->objCount;
					}
					if (keyChildPos < node->objCount)
					{
						rightSib = node->loadChild(keyChildPos + 1, _dataFile, _recSize);
						rightCount = rightSib->objCount;
					}

					// Case 3a: There is a sibling with _minDegree or more keys.
					if (leftCount >= _minDegree || rightCount >= _minDegree)
					{
						// Part of this process is making sure that the
						// child node has minDegree objects.
						childNode->setCount(_minDegree);

						// Bringing the new key from the left sibling
						if (leftCount >= _minDegree)
						{
							// Shuffle the keys and objects up
							size_t ctr = _minDegree - 1;
							for ( ; ctr > 0; ctr--)
							{
								childNode->objects[ctr] = childNode->objects[ctr - 1];
								childNode->children[ctr + 1] = childNode->children[ctr];
								childNode->childNo = ctr + 1; // Thanks steradrian
							}
							childNode->children[ctr + 1] = childNode->children[ctr];

							// Put the key from the parent into the empty space,
							// pull the replacement key from the sibling, and
							// move the appropriate child from the sibling to
							// the target child.
							childNode->objects[0] = node->objects[keyChildPos - 1];
							node->objects[keyChildPos - 1] = leftSib->objects[leftSib->objCount - 1];
							leftSib->objects.resize(leftSib->objCount - 1);
							if (!leftSib->isLeaf)
							{
								childNode->children[0] = leftSib->children[leftSib->objCount];
								leftSib->children.resize(leftSib->objCount);
							}
							--leftSib->objCount;
						}

						// Bringing a new key in from the right sibling
						else
						{
							// Put the key from the parent into the child,
							// put the key from the sibling into the parent,
							// and move the appropriate child from the
							// sibling to the target child node.
							childNode->objects[childNode->objCount - 1] = node->objects[op.first];
							node->objects[op.first] = rightSib->objects[0];
							if (!rightSib->isLeaf)
							{
								childNode->children[childNode->objCount] = rightSib->children[0];
							}

							// Now clean up the right node, shuffling keys
							// and objects to the left and resizing.
							size_t ctr = 0;
							for ( ; ctr < rightSib->objCount - 1; ctr++)
							{
								rightSib->objects[ctr] = rightSib->objects[ctr + 1];
								if (!rightSib->isLeaf)
								{
									rightSib->children[ctr] = rightSib->children[ctr + 1];
								}
							}
							if (!rightSib->isLeaf)
							{
								rightSib->children[ctr] = rightSib->children[ctr + 1];
							}
							rightSib->setCount(rightSib->objCount - 1);
						}
						ret = _delete(childNode, key);
					}

					// Case 3b: All siblings have _minDegree - 1 keys
					else
					{
						TreeNodePtr mergedChild = _merge(node, op.first);
						ret = _delete(mergedChild, key);
					}
				}
			}
		}
		return ret;
	}

	// Finds the location of the predecessor of this key, given
	// the root of the subtree to search. The predecessor is going
	// to be the right-most object in the right-most leaf node.
	NodeKeyLocn BTreeDB::_findPred(TreeNodePtr& node)
	{
		NodeKeyLocn ret(TreeNodePtr(), (size_t)-1);
		TreeNodePtr child = node;
		while (!child->isLeaf)
		{
			child = child->loadChild(child->objCount, _dataFile, _recSize);
		}
		ret.first = child;
		ret.second = child->objCount - 1;
		return ret;
	}

	// Finds the location of the successor of this key, given
	// the root of the subtree to search. The successor is the
	// left-most object in the left-most leaf node.
	NodeKeyLocn BTreeDB::_findSucc(TreeNodePtr& node)
	{
		NodeKeyLocn ret(TreeNodePtr(), (size_t)-1);
		TreeNodePtr child = node;
		while (!child->isLeaf)
		{
			child = child->loadChild(0, _dataFile, _recSize);
		}
		ret.first = child;
		ret.second = 0;
		return ret;
	}

	void BTreeDB::close()
	{
		//flush();
		fflush(_dataFile);
		fclose( _dataFile);
	}

	// Opening the database means that we check the file
	// and see if it exists. If it doesn't exist, start a database
	// from scratch. If it does exist, load the root node into
	// memory.
	bool BTreeDB::open()
	{
		// We're creating if the file doesn't exist.
		SFileHeader sfh;
		bool creating = (0 != _access(_fileName.c_str(), 06));
		_dataFile = fopen(_fileName.c_str(), creating ? "w+b" : "r+b");

		if(creating)
			printf("creating");

		if (0 == _dataFile)
		{
			return false;
		}

		// Create a new node
		bool ret = false;
		if (creating)
		{
			// We *must* have the rec size, key size and
			// and min degree if we are creating. If not
			// supplied, we have to bug out.
			if (_recSize == -1 || _keySize == -1 || _minDegree == -1)
			{
				return false;
			}
			sfh.keySize = _keySize;
			sfh.recSize = _recSize;
			sfh.minDegree = _minDegree;
			sfh.rootPos = sizeof(sfh);
			_nodeSize = sizeof(size_t);						// object count
			_nodeSize += (_minDegree * 2 - 1) * _recSize;	// records
			_nodeSize += _minDegree * 2 * sizeof(long);		// child locations
			_nodeSize += sizeof(byte);						// is leaf?

			// when creating, write the node to the disk.
			// remember that the first four bytes contain
			// the address of the root node.
			if (1 != fwrite((char*)&sfh, sizeof(sfh), 1, _dataFile))
			{
				return false;
			}
			fflush(_dataFile);

			// If creating, allocate a node instead of
			// reading one.
			_root = _allocateNode();
			_root->isLeaf = true;
			_root->loaded = true;
			_root->write(_dataFile);
			ret = true;
		}
		else
		{
			// when not creating, read the root node from the disk.
			memset(&sfh, 0, sizeof(sfh));
			if (1 != fread(&sfh, sizeof(sfh), 1, _dataFile))
			{
				return false;
			}
			else
			{
				_keySize = sfh.keySize;
				_recSize = sfh.recSize;
				_minDegree = sfh.minDegree;
				_nodeSize = sizeof(size_t);						// object count
				_nodeSize += (_minDegree * 2 - 1) * _recSize;	// records
				_nodeSize += _minDegree * 2 * sizeof(long);		// child locations
				_nodeSize += sizeof(byte);						// is leaf?
			}

			// If note creating, just create and read
			// rather than allocating.
			_root = new TreeNode;
			_root->fpos = sfh.rootPos;
			_root->read(_dataFile, _recSize);
			ret = true;
		}
		return ret;
	}

	// This is the external delete function.
	bool BTreeDB::del(const DbObjPtr& key)
	{
		// Determine if the root node is empty.
		bool ret = (_root->objCount != 0);

		// If our root is not empty, call the internal
		// delete method on it.
		ret = ret && _delete(_root, key);

		// If we successfully deleted the key, and there
		// is nothing left in the root node and the root
		// node is not a leaf, we need to shrink the tree
		// by making the root's child (there should only
		// be one) the new root. Write the location of
		// the new root to the start of the file so we
		// know where to look.
		if (ret && _root->objCount == 0 && !_root->isLeaf)
		{
			_root = _root->children[0];
			fseek(_dataFile, 0, SEEK_SET);
			fwrite(&_root->fpos, sizeof(_root->fpos), 1, _dataFile);
			ret = flush();
		}
		return ret;
	}

	// External put method. This will overwrite a key
	// (allowing no duplicates) or insert a new item.
	bool BTreeDB::put(const DbObjPtr& rec)
	{
		if (rec->getSize() != getRecSize())
		{
			return false;
		}
		NodeKeyLocn locn = _search(_root, rec);

		// If we can't find the key, then insert a new
		// record.
		if ((TreeNode*)locn.first == 0 || locn.second == (size_t)-1)
		{
			_insert(rec);
		}

		// If we found the key, update the node with
		// the record.
		else
		{
			locn.first->objects[locn.second] = rec;
			locn.first->write(_dataFile);
		}
		return true;
	}

	// This method retrieves a record from the database
	// given its location.
	bool BTreeDB::get(const NodeKeyLocn& locn, DbObjPtr& rec)
	{
		if ((TreeNode*)locn.first == 0 || locn.second == (size_t)-1)
		{
			return false;
		}
		rec = locn.first->objects[locn.second];
		return true;
	}

	// This method retrieves a record from the database
	// given its key.
	bool BTreeDB::get(const DbObjPtr& key, DbObjPtr& rec)
	{
		NodeKeyLocn locn = _search(_root, key);
		return get(locn, rec);
	}

	// Visit every record in the tree, calling the callback
	// function with the current record, the reference object,
	// and the recursion depth as parameters.
	void BTreeDB::traverse(const DbObjPtr& ref, BTreeDB::traverseCallback cbfn)
	{
		_traverse(_root, ref, cbfn);
	}

	// External method that searches for elements that match the
	// given key. The cfn parameter is a caller-provided callback
	// function used to do the comparison.
	NodeKeyLocn BTreeDB::search(const DbObjPtr& key, compareFn cfn)
	{
		return _search(_root, key, cfn);
	}

	// Structure and methods used for searching the database.
	struct SearchData
	{
		bool started;
		size_t counter;
		DbObjPtr pKey;
		DBOBJVECTOR* pVect;
	};

	// This is the callback function used for findAll traversals.
	// It builds a list of records whose keys match the object given
	// as the search pattern. It starts inserting values once we've found
	// a match, and stops once we've found a record that doesn't match.
	bool BTreeDB::_searchCallback(const DbObjPtr& obj, const DbObjPtr& ref, int /*depth*/)
	{
		SearchData* sd = (SearchData*)ref->getData();
		if (0 == memcmp(obj->getData(), sd->pKey->getData(), min(obj->getSize(), sd->pKey->getSize())))
		{
			if (sd->pVect->size() == sd->pVect->capacity() - 1)
			{
				sd->pVect->reserve(sd->pVect->capacity() * 2);
			}
			sd->pVect->push_back(obj);
			sd->started = true;
			++sd->counter;
		}
		else if (sd->started)
		{
			return false;
		}
		return true;
	}

	// This method finds all records in the database that match
	// the given key. Note that this doesn't necessarily compare
	// the entire key. This is for finding all keys that match
	// "ABC%", f'rinstance.
	void BTreeDB::findAll(const DbObjPtr& key, DBOBJVECTOR& results)
	{
		results.clear();
		SearchData sd = { false, 0, key, &results };
		DbObjPtr pRef = new DbObj(&sd, sizeof(sd));
		_traverse(_root, pRef, _searchCallback);
		memcpy(&sd, pRef->getData(), sizeof(sd));
	}

	// This method finds the record following the one at the
	// location given as locn, and copies the record into rec.
	// The direction can be either forward or backward.
	bool BTreeDB::seq(NodeKeyLocn& locn, DbObjPtr& rec, ESeqDirection sdir)
	{
		switch (sdir)
		{
		case ESD_FORWARD:
			return _seqNext(locn, rec);

		case ESD_BACKWARD:
			return _seqPrev(locn, rec);
		}

		return false;
	}

	// Find the next item in the database given a location. Return
	// the subsequent item in rec.
	bool BTreeDB::_seqNext(NodeKeyLocn& locn, DbObjPtr& rec)
	{
		// Set up a couple of convenience values
		bool ret = false;
		TreeNodePtr node = locn.first;
		size_t lastPos = locn.second;
		bool goUp = false;	// indicates whether or not we've exhausted a node.

		// If we are starting at the beginning, initialise
		// the locn reference and return with the value set.
		// This means we have to plunge into the depths of the
		// tree to find the first leaf node.
		if ((TreeNode*)node == 0)
		{
			node = _root;
			while ((TreeNode*)node != 0 && !node->isLeaf)
			{
				node = node->loadChild(0, _dataFile, _recSize);
			}
			if ((TreeNode*)node == 0)
			{
				return false;
			}
			rec = node->objects[0];
			locn.first = node;
			locn.second = 0;
			return true;
		}

		// Advance the locn object to the next item

		// If we have a leaf node, we don't need to worry about
		// traversing into children ... only need to worry about
		// going back up the tree.
		if (node->isLeaf)
		{
			// didn't visit the last node last time.
			if (lastPos < node->objCount - 1)
			{
				rec = node->objects[lastPos + 1];
				locn.second = lastPos + 1;
				return true;
			}
			goUp = (lastPos == node->objCount - 1);
		}

		// Not a leaf, therefore need to worry about traversing
		// into child nodes.
		else
		{
			node = node->loadChild(lastPos + 1, _dataFile, _recSize);
			while ((TreeNode*)node != 0 && !node->isLeaf)
			{
				node = node->loadChild(0, _dataFile, _recSize);
			}
			if ((TreeNode*)node == 0)
			{
				return false;
			}
			rec = node->objects[0];
			locn.first = node;
			locn.second = 0;
			return true;
		}

		// Finished off a leaf, therefore need to go up to
		// a parent.
		if (goUp)
		{
			size_t childNo = node->childNo;
			node = node->parent;
			while ((TreeNode*)node != 0 && childNo >= node->objCount)
			{
				childNo = node->childNo;
				node = node->parent;
			}
			if ((TreeNode*)node != 0)
			{
				locn.first = node;
				locn.second = childNo;
				ret = true;
				rec = node->objects[childNo];
			}
		}
		return ret;
	}

	// Find the previous item in the database given a location. Return
	// the item in rec.
	bool BTreeDB::_seqPrev(NodeKeyLocn& locn, DbObjPtr& rec)
	{
		// Set up a couple of convenience values
		bool ret = false;
		TreeNodePtr node = locn.first;
		size_t lastPos = locn.second;
		bool goUp = false;	// indicates whether or not we've exhausted a node.

		// If we are starting at the end, initialise
		// the locn reference and return with the value set.
		// This means we have to plunge into the depths of the
		// tree to find the first leaf node.
		if ((TreeNode*)node == 0)
		{
			node = _root;
			while ((TreeNode*)node != 0 && !node->isLeaf)
			{
				node = node->loadChild(node->objCount, _dataFile, _recSize);
			}
			if ((TreeNode*)node == 0)
			{
				return false;
			}
			locn.first = node;
			locn.second = node->objCount - 1;
			rec = node->objects[locn.second];
			return true;
		}

		// Advance the locn object to the next item

		// If we have a leaf node, we don't need to worry about
		// traversing into children ... only need to worry about
		// going back up the tree.
		if (node->isLeaf)
		{
			// didn't visit the last node last time.
			if (lastPos > 0)
			{
				locn.second = lastPos - 1;
				rec = node->objects[locn.second];
				return true;
			}
			goUp = (lastPos == 0);
		}

		// Not a leaf, therefore need to worry about traversing
		// into child nodes.
		else
		{
			node = node->loadChild(lastPos, _dataFile, _recSize);
			while ((TreeNode*)node != 0 && !node->isLeaf)
			{
				node = node->loadChild(node->objCount, _dataFile, _recSize);
			}
			if ((TreeNode*)node == 0)
			{
				return false;
			}
			locn.first = node;
			locn.second = node->objCount - 1;
			rec = node->objects[locn.second];
			return true;
		}

		// Finished off a leaf, therefore need to go up to
		// a parent.
		if (goUp)
		{
			size_t childNo = node->childNo;
			node = node->parent;
			while ((TreeNode*)node != 0 && childNo >= node->objCount)
			{
				childNo = node->childNo;
				node = node->parent;
			}
			if ((TreeNode*)node != 0)
			{
				locn.first = node;
				locn.second = childNo - 1;
				rec = node->objects[locn.second];
				ret = true;
			}
		}
		return ret;
	}

	// This method flushes all loaded nodes to the file and
	// then unloads the root node. So not only do we commit
	// everything to file, we also free up any memory previously
	// allocated.
	bool BTreeDB::flush()
	{
		bool ret = _flush(_root, _dataFile);
		if (ret)
		{
			fflush(_dataFile);
		}

		// Unload each of the root's childrent. If we
		// unload the root itself, we lose the use of
		// the tree.
		for (size_t ctr = 0; _root->isLeaf && ctr < _root->objCount; ctr++)
		{
			TreeNodePtr pChild = _root->children[ctr];
			if ((TreeNode*)pChild != 0)
			{
				_root->children[ctr]->unload();
			}
		}
		return ret;
	}
}