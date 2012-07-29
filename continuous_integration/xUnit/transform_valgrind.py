#!/usr/bin/env python

# vim: set sw=4 ts=4 autoindent expandtab:

'''
Script to convert valgrind XML output to xUnit XML output.

An example run looks like this (all options may not be required):
valgrind --track-fds=yes --leak-check=no --track-origins=yes --xml=yes --xml-file=log/valgrind-out.xml <app and args>
./transform_valgrind.py -i ../config/current/log/valgrind-out.xml -o ../config/current/log/valgrind.TEST.xml -d ${DURATION}
'''

import warnings
with warnings.catch_warnings():
    warnings.simplefilter("ignore")
    import sys
    from optparse import OptionParser
    from xml.dom import minidom, Node
    from xml.dom.minidom import getDOMImplementation
    from socket import gethostname
    from datetime import datetime
    import re

def buildFrameName(uniqueid, dirName, fileName, line, funcName, default):
    '''
    Build a string to identify a failure.
    '''
    retval = str()
    if dirName:
        m = re.match(r"^.*/(cpu/.*$)", dirName)
        if m:
            retval = m.group(1)
        else:
            retval = dirName
        retval = retval + "/"

    if fileName:
        retval = retval + fileName
        if line:
            retval = retval + ":" + line

    if funcName:
        if len(retval) > 0:
            retval = retval + " " + funcName
        else:
            retval = funcName

    if len(retval) == 0:
        retval = default

    if not retval:
        print "ERROR: ", uniqueid, dirName, fileName, line, funcName, default
        sys.exit()

    return retval


def processError(failures, error):
    '''
    @param failures A dictionary in which the key is a 2-tuple, first element is
        the 'what' field constructed from either the <what> or <xwhat> elements
        of the valgrind XML file, and the second is the complete stacktrace string
        constructed from the concatenation of all the <frame> elements of the
        valgrind XML file. The value stored is a record (i.e.  dictionary)
        containing fields 'what' (a string), 'count' (an int), 'uniqueIds' (a
        list of ints from the <unique> element of the valgrind XML).
    @param error
        The XML DOM node of the error in the valgrind output.
    '''
    unique = getTextFromNode(getFirstChildWithName(error, "unique"))
    whatNode = getFirstChildWithName(error, "what")
    what = getTextFromNode(whatNode)
    if not what:
        xwhatNode = getFirstChildWithName(error, "xwhat")
        what = getTextFromNode(getFirstChildWithName(xwhatNode, "text"))
        if not what:
            raise Exception("Error: %s must have either what or xwhat" % unique);
    kindNode = getFirstChildWithName(error, "kind")
    kind = getTextFromNode(kindNode)
    stackNode = getFirstChildWithName(error, "stack")

    stackFrames = []
    for stackFrame in getChildrenWithName(stackNode, "frame"):
        obj = getTextFromNode(getFirstChildWithName(stackFrame, "obj"))
        instructionPtr = getTextFromNode(getFirstChildWithName(stackFrame, "ip"))
        if obj:
            default = obj + "@" + instructionPtr
        else:
            default = instructionPtr # note: <ip> is required to be present in the valgrind XML

        function = getTextFromNode(getFirstChildWithName(stackFrame, "fn"))
        fileName = getTextFromNode(getFirstChildWithName(stackFrame, "file"))
        dirName = getTextFromNode(getFirstChildWithName(stackFrame, "dir"))
        line = getTextFromNode(getFirstChildWithName(stackFrame, "line"))

        stackFrames.append(buildFrameName(unique, dirName, fileName, line, function, default))

    if len(stackFrames) == 0:
        stack = str()
    else:
        stack = str("\n").join(stackFrames)

    if failures.has_key((kind, stack)):
        failures[(kind, stack)]['count'] = failures[(kind, stack)]['count'] + 1
        failures[(kind, stack)]['uniqueIds'].append(unique)
    else:
        failures[(kind, stack)] = {'count' : 1, 'what' : what, 'uniqueIds' : [unique]}


def fillDomImpl(duration, failures, testsuite, domImpl):
    '''
    Iterates through failures, adding each one's information to the XML DOM
    implementation object.
    '''
    for f in failures:
        testcase = domImpl.createElement("testcase")
        testcase.setAttribute("classname", "valgrind-" + f['kind'])
        testcase.setAttribute("name", f['stack'].split('\n',1)[0])
        testcase.setAttribute("time", "%s" % (duration))

        failure = domImpl.createElement("failure")
        if f['what'] != None:
            occurrences = " (%s occurrence%s. Unique valgrind entries %s)" %\
                (f['count'], "s" if f['count'] > 1 else "", f['uniqueIds'])
            failure.setAttribute("message", f['what'] + occurrences)
        else:
            raise Exception("Must have what or xwhat %s" % failures[k]['uniqueIds'])
        failure.setAttribute("type", f['kind'])
        stack = domImpl.createCDATASection(f['stack'])
        failure.appendChild(stack) # the full stack trace

        testcase.appendChild(failure)

        testsuite.appendChild(testcase)


def getTextFromNode(node):
    """
    Get the text out of a node, which is assumed to be the first child.
    If there is no text or no child, None is returned.
    """
    if node == None:
        return None

    for child in node.childNodes:
        return child.data
    return None

def getChildrenWithName(node, name):
    for child in node.childNodes:
        if child.nodeType == Node.ELEMENT_NODE and child.nodeName == name:
            yield child
    return

def getFirstChildWithName(node, name):
    for child in node.childNodes:
        if child.nodeType == Node.ELEMENT_NODE and child.nodeName == name:
            return child

def transformValgrind(valgrindDoc, junitOut, duration):
    """
    Transform the valgrind document and write it out to junitOut.

    @param valgrindDoc the valgrind document to read
    @param junitOut the file object to write to
    @param duration the elapsed time of the test in seconds
    @return the exit code (0 is success)
    """
    rootNode = valgrindDoc.documentElement
    if None == rootNode:
        print >>sys.stderr, "Cannot find root node!"
        return 1

    impl = getDOMImplementation()
    junitDoc = impl.createDocument(None, "testsuite", None)
    testsuite = junitDoc.documentElement
    numErrors = 0
    failures = dict()
    for error in getChildrenWithName(rootNode, "error"):
        processError(failures, error)
        numErrors = numErrors + 1

    # sort failures by number of occurrences, most to least
    sortedFailures = list()
    for f in failures.keys():
        valueRec = failures[f];
        valueRec['kind'] = f[0]
        valueRec['stack'] = f[1]
        sortedFailures.append( valueRec )
    sortedFailures.sort(key=(lambda x : x['count']), reverse=True), 

    fillDomImpl(duration, sortedFailures, testsuite, junitDoc)

    numTests = numErrors
    if numErrors == 0:
        # create success
        testcase = junitDoc.createElement("testcase")
        testcase.setAttribute("classname", "valgrind")
        testcase.setAttribute("name", "no-errors")
        testcase.setAttribute("time", "%s" % (duration))
        testsuite.appendChild(testcase)
        numTests = 1

    testsuite.setAttribute("errors", "%s" % numErrors)
    testsuite.setAttribute("tests", "%s" % numTests)
    testsuite.setAttribute("failures", "0")
    testsuite.setAttribute("name", "valgrind")
    testsuite.setAttribute("hostname", gethostname())
    testsuite.setAttribute("time", "%s" % (duration))
    testsuite.setAttribute("timestamp", datetime.today().isoformat())

    junitDoc.writexml(junitOut)

def main(argv=None):
    """
    @return the exit status
    """
    if argv is None:
        argv = sys.argv

    parser = OptionParser()
    parser.add_option("-i", "--input", dest="input", help="input from valgrind xml file  (required)")
    parser.add_option("-o", "--output", dest="output", help="output to junit xml file (required)")
    parser.add_option("-d", "--duration", dest="duration", type="int", help="duration of test run in seconds (required)")
    (options, args) = parser.parse_args(argv)

    if options.input == None:
        parser.error("Input is required")
    if options.output == None:
        parser.error("Output is required")
    if options.duration == None:
        parser.error("Duration is required")

    doc = minidom.parse(options.input)

    junitOut = file(options.output, "w")

    return transformValgrind(doc, junitOut, options.duration)

if __name__ == "__main__":
    sys.exit(main())
