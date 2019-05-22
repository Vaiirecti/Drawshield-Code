<?php

include "grammar.inc";

class productions {
    private $parseTree;
    private $ruleset;
    private $messages;
    private $xml;

    public function __construct($language, $xml) {
        $this->ruleset = new grammar ("$language/blazonry.txt"); // should be called ruleset.txt?
        $this->messages = array();
        $this->xml = $xml;
    }

    protected function getSubItems($pPtr, $numMatched) {
    
        $subItems = array();
        for ($i = 0; $i < $numMatched; $i++) {
            if (!$this->parseTree[$pPtr]->hasAttribute("discard"))
                $subItems[] = $this->parseTree[$pPtr];
            $pPtr++;
        }
        return $subItems;
    }

    public function getMessages() {
        return $this->messages;
    }

    protected function firstAndLast($str) {
        $nums = explode('-',$str);
        return $nums[0] . '-' . $nums[count($nums)-1];
    }
    
    protected function testProduction($symbolList, $startPoint) {
    
        // recursion for alternatives
        if (is_array($symbolList)) {
            foreach ($symbolList as $alternate) {
                if ($matched = $this->testProduction($alternate, $startPoint))  {
                    return $matched;
                }
            }
            return false;
        }
    
        $symbolPtr = 0;
        $treePtr = $startPoint;
        $matchedPTIs = 0;
        $symbols = preg_split('/\s+/', $symbolList, -1, PREG_SPLIT_NO_EMPTY);
        $repeatable = 0;
    
        while (true) {
            $allOptional = false;
            if ($treePtr >= count($this->parseTree)) {
                // not an error if all remaining symbols are optional...
                $allOptional = true;
                for (; $symbolPtr < count($symbols); $symbolPtr++) {
                    if ($symbols[$symbolPtr][0] != '?' && $repeatable < 1) {
                        $allOptional = false;
                        break;
                    }
                }
                if (!$allOptional)
                    return false; // not matched all symbols, but no input left
            }
            if ($allOptional || $symbolPtr >= count($symbols)) { // all symbols match
                // if any of the matched PTIs were arrays, replace it by the matched array item
                for ($i = $startPoint; $i < $startPoint + $matchedPTIs; $i++) {
                    if (is_array($this->parseTree[$i])) {
                        // echo "Picking matched item from array\n";
                        foreach ( $this->parseTree[$i] as $array_item) {
                            if ($array_item->hasAttribute("matched")) {
                                $newPTI = $array_item;
                                array_splice($this->parseTree, $i, 1, [$newPTI]);
                                break;
                            }
                        }
                    } 
                }
                return $matchedPTIs; // only get here if all symbols match
            }
            if (!$repeatable) { // process flags
                $symbolToMatch = $symbols[$symbolPtr];
                $optional = (strpos($symbolToMatch[0],'?') !== false );
                $repeatable = (strpos($symbolToMatch,'*') !== false ) ? 1 : 0;
                $discard =  (strpos($symbolToMatch,'^') !== false);
                $symbolToMatch = str_replace(array('<','>','/','?','^','*'),'',$symbolToMatch);
            }
            // echo "Comparing (" . $this->parseTree[$treePtr]->symbol . ') & ('. $symbolToMatch . ")\n";
            // Compare the current two symbols
            $found = false;
            if (is_array($this->parseTree[$treePtr])) {
                foreach($this->parseTree[$treePtr] as $altPTI) { 
                    if ($altPTI->nodeName == $symbolToMatch) {
                        if ($optional) $altPTI->setAttribute("optional","true");
                        $altPTI->setAttribute("matched","true");
                        $found = true;
                    } else {
                        if ($discard) $altPTI->setAttribute("discard","true");
                    }
                }
            } else {
                if ($this->parseTree[$treePtr]->nodeName == $symbolToMatch) {
                    if ($optional) $this->parseTree[$treePtr]->setAttribute("optional","true");
                    if ($discard) $this->parseTree[$treePtr]->setAttribute("discard","true");
                    $found = true;
                }
            }
            $treeInc = 1;
            if (!$found) {
                //echo "No match at this location\n";
                if ($optional || $repeatable > 1) {    // This is OK, just
                    $treeInc = 0; // don't advance tree pointer
                    $repeatable = 0;
                } else {
                    return false; 
                }
            }    // either found, or optional
            if ($repeatable) {
                $repeatable++; // don't advance pointer, increase count
            } else {
                $symbolPtr++;
            }
            $treePtr += $treeInc; 
            $matchedPTIs += $treeInc;   
        }
        // shouldn't get here
        return false;
    }
    
    public function applyRules( $parseTree ) {
        $this->parseTree = $parseTree;
        do {
            $numMatched = false;
            // For each entry in the list of grammar productions
            for ($gPtr = 0; $gPtr < $this->ruleset->numProductions; $gPtr++) {
                $newSymbol = $this->ruleset->productions[$gPtr][0];
                $symbolList = $this->ruleset->productions[$gPtr][1];
                // echo "Looking for $newSymbol\n";
                $parseTreeLength = count($this->parseTree);
                // Starting from each item still in the parse tree
                for ($pPtr = 0; $pPtr < $parseTreeLength; $pPtr++) {
                    if ($numMatched = $this->testProduction($symbolList, $pPtr)) {
                        $linkedTokens = '';
                        $linkedLines = $this->parseTree[$pPtr]->getAttribute(blazonML::A_LINENUMBER);
                        $endLine = $this->parseTree[$pPtr + $numMatched -1]->getAttribute(blazonML::A_LINENUMBER);
                        if ($endLine != $linkedLines) {
                            $linkedLines = $this->firstAndLast("$linkedLines-$endLine");
                        }
                        for ($i = $pPtr; $i < $pPtr + $numMatched; $i++) {
                            if ($this->parseTree[$i]->getAttribute(blazonML::A_TOKENS)) {
                                $linkedTokens .= $this->parseTree[$i]->getAttribute(blazonML::A_TOKENS) . ' ';
                            }
                        }
                        $linkedTokens = trim($linkedTokens);
                        $newPTI = $this->xml->makeNode($newSymbol, '', $linkedTokens, $linkedLines);
                        foreach ($this->getSubItems($pPtr, $numMatched ) as $subItem) 
                            $newPTI->appendChild($subItem);
                        array_splice($this->parseTree, $pPtr, $numMatched, [$newPTI]);
                        if (count( $this->ruleset->productions[$gPtr] ) > 2) {
                            $this->messageList[] = $this->ruleset->productions[$gPtr][2] . ": $linkedTokens near $linkedLines";
                        }
                        break 2;
                    }
                // echo $numMatched ? "true" : "false" . " $gPtr of $grammarLength $pPtr of $parseTreeLength \n";
                }
            }
            //break;
        } while ($numMatched);
        return $this->parseTree;
    }
    
    
}