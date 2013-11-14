""" Author: Sibel Adali, last modified: April 14, 2013
    Name: Btree program. 

    To run this program: Needs python 2.7. Type:

        python btree_adali.py input_file_name

    It assumes the existence of file scores_byname.txt.

    Each leaf node stores NODECAP-1 tuples, for each
    tuple there is a key value and a page id, the address of the tuple
    with the given key value. Also, there is a pointer to the next sibling
    at each leaf node.

    Each internal node stores NODECAP pointers and NODECAP-1 keys. Check
    the book for details.

"""

import sys

class IndexedTuple(object):
    """ The basic information to store for each tuple, 
    the key values and the page address.

    """

    def __init__ (self, key, pageid):
        self.key = key
        self.pageid = pageid
    def val (self):
        return self.key
    def loc (self):
        return self.pageid
    def __str__(self):
        return "(%s,%s,%s,%s):%s"\
            %(self.key[0],self.key[1],self.key[2],self.key[3],self.pageid)

class LeafNode(object):
    """ Each leaf node stores a list of indexed tuple objects, 
    and a sibling which is a pointer to the next leaf node.

    """

    def __init__ (self, tuples, sibling=None):
        self.tuples = tuples
        self.sibling = sibling
    def isleaf (self):
        return True
    def smallest (self):
        return self.tuples[0].val()
    def set_sibling (self, sibling) :
        self.sibling = sibling
    def search_key_full_eq (self, key) : 
        """ Find all leaf nodes with values equal to the given key. """
        found = []
        for i in xrange( len(self.tuples) ):
            if self.tuples[i].val() == key:
                found.append ( (self.tuples[i].val(), self.tuples[i].loc()) )
        return found
    def __str__ (self) :
        tuple_str = ""
        for tuple in self.tuples:
            tuple_str += "(%s) " %(str(tuple))
        return tuple_str


class InternalNode(object):
    """ Each internal node stores a list of keys and a list of pointers.
        We should have len(pointers) = 1+len(keys).
        pointer[0] points to values z s.t.:  z < key[0]
        pointer[1] points to values z s.t.: key[0] <= z < key[1]
        ...
        pointer[-1] points to values z s.t. key[-1] <= z

    """

    def __init__ (self, keys, pointers):
        self.keys = keys
        self.pointers = pointers
    def isleaf (self):
        return False
    def smallest (self):  
        """ Return the smallest value in the whole subtree. """
        return self.pointers[0].smallest() 
    def search_key_full_eq(self, key):
        """ Find all leaf nodes with values equal to the given key. """
        for i in xrange(len(self.keys)):
            if self.keys[i] > key:
                if i > 0:
                    print "   visited internal ==>", self.keys[i] 
                    return self.pointers[i]
                else:
                    print "   visited internal ==>", self.keys[0] 
                    return self.pointers[0]
        print "   visited internal ==>", self.keys[-1] 
        return self.pointers[-1]
    def __str__ (self) :
        key_str = ""
        for key in self.keys:
            key_str += "(%s,%s,%s,%s) " %(key[0], key[1], key[2], key[3])
        return key_str

def create_internal_node( cur_tree_level ):
    """ Given a list of nodes at the lower level, find the keys that
        are division points by finding the lowest entries for all
        nodes starting with the second node.

    """

    keys = []
    for node in cur_tree_level[1:]:
        keys.append ( node.smallest() )
    return InternalNode( keys, cur_tree_level )

def search ( root, key ):
    """ Searches for the equality of a key starting with the root. """

    print "Searching i1 for equality (%s,%s,%s,%s)"\
        %(key[0], key[1], key[2], key[3])
    cur_node = root
    nodes_visited = 1
    while not cur_node.isleaf():
        cur_node = cur_node.search_key_full_eq( key )
        nodes_visited += 1
    found = cur_node.search_key_full_eq( key )

    print "Total nodes visited:", nodes_visited
    pages = set()
    if len(found) == 0:
        print "Record not found."
    else:
        print "Record found.",
        for (key, pageid) in found:
            pages.add(pageid)
            print "%s pageid:%s " %(key, pageid), 
        print
    print "Total disk pages:", len(pages)
    print ""


def print_tree( root, level ):
    """ Prints a whole tree for debugging purposes. """

    if not root.isleaf():
        print level*"==" + "==> ", str(root), "pointers", len(root.pointers)
        for p in root.pointers:
            print_tree ( p, level+1 )
    else:
        print level*"==" + "==> ", 
        for t in root.tuples:
            print str(t), 
        print ""

if __name__ == "__main__":
    NODECAP = 60  ## the max capacity of the nodes

    ## pack the data into the leaf nodes first
    cur_tup = [] 
    leaf_nodes = []  
    for line in open("scores_byname.txt"):
        m = line.strip("\n").split("\t")
        name = m[0].strip()
        showid = m[1].strip()
        finalscore = m[2].strip()
        place = m[3].strip()
        pageid = m[4].strip()
        if len(cur_tup) >= NODECAP-1: 
            leaf_nodes.append( LeafNode(cur_tup) )
            cur_tup = []
        t = IndexedTuple( [name,showid,finalscore,place], pageid)
        cur_tup.append(t)
    if len(cur_tup) != 0:
        leaf_nodes.append( LeafNode(cur_tup) )

    ## Set the sibling pointers to point to the next one.
    for i in xrange(len(leaf_nodes)-1):
        leaf_nodes[i].set_sibling( leaf_nodes[i+1] )

    print "Done generating %d leaf nodes..." %len(leaf_nodes)
    print "Moving to the internal nodes."
    
    ## generate upper levels of the tree, bottom up.

    cur_tree_level = leaf_nodes

    while len(cur_tree_level) > 1:   ## loop until the root is generated
        ## find the number of nodes in the next level
        if len(cur_tree_level)%NODECAP == 0:
            num_nodes = len( cur_tree_level )/NODECAP
        else:
            num_nodes = 1 + len( cur_tree_level )/NODECAP
    
        new_level = []
        start = 0
        for i in xrange(num_nodes):
            end = (i+1)*NODECAP 
            new_level.append (create_internal_node(cur_tree_level[start:end]))
            start = end
    
        print "Created %d nodes at the next level" % len(new_level)
        cur_tree_level = new_level

    print "Done generating the b-tree"
    root = cur_tree_level[0]

    #print_tree(root, 0)

    print "Search results for Sibel Adali (adalis)"
    cmd_file = sys.argv[1]
    
    for line in open(cmd_file):
        m = line.strip("\n").split("\t")

        search(root, m)

