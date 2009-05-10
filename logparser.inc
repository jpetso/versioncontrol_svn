<?php

class VersioncontrolSvnLogHandler implements CLIParser, Iterator {
  /**
   *
   * @var SvnInfo
   */
  public $aggregateInfo;
  /**
   *
   * @var SvnInstance
   */
  protected $instance;

  public $revisions = array();
  protected $revision;

  // protected $outputHandles = array();

  /**
   *
   * @var SvnInfo
   */
  protected $opRevInfo;
  protected $opRevAggregator = array();
  protected $log, $info;

  protected $allActions = array();
  protected $actions;

  protected $firstRev, $lastRev, $currentRev;
  protected $ready = FALSE;
  
  public function __construct(SvnInstance $instance, $first_rev, $last_rev) {
    $this->instance = $instance;
    $this->firstRev = $first_rev;
    $this->lastRev = $last_rev;
    $this->log = new VersioncontrolSvnLogParser();
  }
  
  public function openOutputHandle() {
    return $this->log->openOutputHandle();
  }

  /**
   * Implementation of CLIParser::parseOutput().
   *
   * This method is called once, on the firing of the SvnLog command's output
   * parser during execution. We iterate over the contents of the log output and
   * determine what additional information we'll need from svn, queue all those
   * requests up into a command, then fire them as needed either at the end,
   * or during iteration.
   *
   * @return VersioncontrolSvnLogHandler
   */
  public function parseOutput() {
    $this->log->parseOutput();
    // Outer revisions loop; each iteration deals encomapsses a single revision.
    foreach ($this->log as $revision) {
      // Inner revisions loop; each iteration encompasses with a single path on
      // the current revision.
      $this->aggregateInfo[$revision['rev']] = $this->instance->svn('info');
      $this->info = $this->aggregateInfo[$revision['rev']];
      foreach ($revision['paths'] as $path => $path_info) {
        // Add info requests for copyfrom and delfrom info, which will be used
        // later to fill out item metadata.
        foreach (array('copyfrom', 'delfrom') as $from) {
          if (isset($path_info[$from])) {
            $this->info->target($path_info[$from]['path'], $path_info[$from]['rev'], TRUE);
          }
        }

        // By bundling many URL detail retrievals in one `svn info` call, we
        // can keep the number of 'svn info' invocations down to a minimum.
        // Doing this one by one would be a major blow to performance.
        if ($path_info['action'] & VERSIONCONTROL_SVN_ACTION_CHANGE) {
          $this->info->target($path, $revision['rev'], TRUE);
          if ($path_info['action'] == VERSIONCONTROL_SVN_ACTION_MODIFY) {
            // Modified items require the special operative/peg rev syntax to
            // svn info, so we queue them into their own special per-rev object.
            $this->queueOpRevTarget($path, $revision['rev'], $revision['rev'] - 1);
          }
        }
        if ($path_info['action'] == VERSIONCONTROL_SVN_ACTION_DELETE_SIMPLE) {
          // Ugly delete actions do not originate in the previous revision, so
          // querying there would result in an error. Only simple deletes here
          $this->info->target($path, $revision['rev'] - 1, TRUE);
        }
      } // end inner revisions loop
    } // end outer revisions loop
//    // All queued up - fire it and store the resulting info parser.
//    $this->info = $this->aggregateInfo->execute();
//    // Kill aggregateinfo to free memory - it can easily contain LOTS of objects
//    unset($this->aggregateInfo);
    $this->ready = TRUE;
    return $this;
  }

  protected function queueOpRevTarget($target, $peg_rev, $op_rev) {
    if (empty($this->opRevAggregator[$peg_rev])) {
      $this->opRevAggregator[$peg_rev] = $this->instance->svn('info');
      $this->opRevInfo = $this->opRevAggregator[$peg_rev];
      $this->opRevInfo->revision($op_rev);
    }
    $this->opRevInfo->target($target, $peg_rev, TRUE);
  }

  /**
   * Implementation of CLIParser::clear(). Empty because this parser can't be
   * cleared.
   */
  public function clear() {}

  /**
   * Implementation of Iterator::rewind().
   */
  public function rewind() {
    if (!$this->ready) {
      throw new Exception('The parser has not yet collected all the necessary data, and is not prepared for iteration.', E_RECOVERABLE_ERROR);
    }
    $this->currentRev = $this->firstRev;
  }

  /**
   * Implementation of Iterator::key().
   */
  public function key() {
    return $this->currentRev;
  }

  /**
   * Implementation of Iterator::valid().
   */
  public function valid() {
    return $this->ready ? $this->currentRev <= $this->lastRev : FALSE;
  }

  /**
   * Implementation of Iterator::next().
   */
  public function next() {
    $this->opRevAggregator[$this->currentRev] = NULL;
    $this->aggregateInfo[$this->currentRev] = NULL;
//    unset($this->revision);
    $this->currentRev++;
  }

  protected function pointersToCurrentRev() {
    // Retrieve the revision data from the cached xml output of svn log.
    $this->revision = $this->log->seek($this->currentRev);
    $this->info = $this->aggregateInfo[$this->currentRev]->execute();
    $this->actions = &$this->revision['paths'];
    $this->opRevInfo = &$this->opRevAggregator[$this->currentRev];
    if ($this->opRevInfo instanceof SvnInfo) {
      $this->opRevInfo->clear(SvnCommand::PRESERVE_ALL);
    }
  }

  /**
   * Implementation of Iterator::current().
   *
   * Builds the array versioncontrol_svn wants on the fly, using the stored
   * seekable log and info iterators, as well as any necessary stored opRev
   * requests.
   */
  public function current() {
    $this->pointersToCurrentRev();
    // Retrieve all info items for the current revision.
    foreach ($this->info->seekRev($this->currentRev) as $path => $info_item) {
      // $path = $info_item['path']; // REFACTORED OUT
      if (empty($this->actions[$path])) {
        continue; // means we're on one that was called up by somethin else
      }
      $this->actions[$path]['current_item'] = self::itemFromInfo($info_item);

      if (isset($this->actions[$path]['copyfrom'])) { // can happen for 'A' or 'R' actions
        // Yay, we can have the source item without invoking the binary.
        $this->actions[$path]['source_item'] = $this->actions[$path]['current_item'];
        $this->actions[$path]['source_item']['path'] = $this->actions[$path]['copyfrom']['path'];
        $this->actions[$path]['source_item']['rev'] = $this->actions[$path]['copyfrom']['rev'];
        unset($this->actions[$path]['copyfrom']); // not needed anymore
      }
    }

    // Now we retrieve all source items of 'M' actions from our aggregated
    // output handler. Mind that these can have different paths than the current
    // item.
    if (!empty($this->opRevInfo) && $this->opRevInfo instanceof SvnInfo) {
      foreach ($this->opRevInfo->execute() as $info_item) {
        $this->actions[$info_item['path']]['source_item'] = self::itemFromInfo($info_item);
      }
    }

    // Bonus feature: Recognize moves and copies by inspecting the source item
    // of added items and matching it to a deleted one. We can it this way
    // because if the icon really were an added one then it wouldn't have
    // a source item at all. The nice thing is that we can even get rid
    // of more 'svn info' invocations by removing the corresponding 'D' actions.
    foreach ($this->actions as &$action) {
      // TODO check and make sure this is right - encomapsses both A and R
      // FIXME this section needs work. More of it needs to be resolved back in
      // the xml parsing stage.
      if ($action['action'] & VERSIONCONTROL_SVN_ACTION_CHANGE && isset($action['source_item'])) {
        // Search through all other items if they contain the source item
        // of this add action.
        foreach ($this->revision['paths'] as $other_path => $other_path_info) {
          // Only consider most recent delete actions with a matching path.
          if ($this->actions[$other_path]['action'] & VERSIONCONTROL_SVN_ACTION_DELETE
              && $action['source_item']['rev'] == ($action['current_item']['rev'] - 1)
              && $other_path == $action['source_item']['path']) {
            // Hah! gotcha. Die, delete action, die! Instead, the "deleted" item
            // is just going to be the source item of the now merged action.
            // ...that's right, what we've got here is a move.
            unset($this->revision['paths'][$other_path]);
            // unset($this->actions[$other_path]);
            $action['action'] = VERSIONCONTROL_SVN_ACTION_MOVE;
            break;
          }
        }
        // If the item was not moved, it must have been copied.
        // (Otherwise there would be no source item.)
        if ($action['action'] != VERSIONCONTROL_SVN_ACTION_MOVE) {
          $action['action'] = VERSIONCONTROL_SVN_ACTION_COPY;
        }
      }
    }

    // Fourth step: retrieve the source items of the given path,
    // and construct the $action info array for that path.
    foreach ($this->actions as $path => &$action) {
      // In case there was a modified item of which the parent was moved
      // to a new location just in this very revision, it has been missing
      // from the retrieved modified items further above, which means the
      // remaining ones could not be mapped to their actions.
      // In order to map them anyways, we have to retrieve them one by one -
      // which is slow, but will hardly ever occur anyways.
      //
      // REFACTORED *** this should never EVER happen (Can't remember why, now...damn)
//      if ($actions[$path]['action'] == 'M' && !isset($actions[$path]['source_item'])) {
//        $source_items = svnlib_info_cached($actions[$path]['url'], $revision['rev'], $revision['rev'] - 1);
//        if ($source_items) {
//          $source_item = reset($source_items); // first item
//          $actions[$path]['source_item'] = self::itemFromInfo($source_item);
//        }
//      }

      // Ok, that was the easy part - 'M' and 'A' are covered and should now
      // have a current item (in all cases) and source item (if it existed).
      // The hard part is 'D'.
      if ($action['action'] & VERSIONCONTROL_SVN_ACTION_DELETE) {
        // If the original type was 'R', we essentially have two actions:
        // one delete action and one add/move/copy action. We now have the data
        // for the latter one, but we can also get the data for the delete action
        // by making the algorithm believe that the action is 'D', not 'A'.
        // Thus, we store the deleted item parent info into different slot.
        $into = $action['action'] == VERSIONCONTROL_SVN_ACTION_REPLACE ? 'replaced_item' : 'source_item';
        if (!empty($action['delfrom'])) {
          $source_item = $this->info->seekBoth($action['delfrom']['rev'], $this->instance->getRepoRoot() . $action['delfrom']['path']);
          $action[$into] = self::itemFromInfo($source_item);
          $action['current_item'] = $action[$into];
          $action['current_item']['path'] = $path;
          $action['current_item']['rev'] = $this->currentRev;
          // type is ok, as it's the same as in the source item.
        }
        elseif ($source_item = $this->info->seekBoth($this->currentRev - 1, $this->instance->getRepoRoot() . $path)) {
          $action[$into] = self::itemFromInfo($source_item);
          $action['current_item'] = $action[$into];
          $action['current_item']['rev'] = $this->currentRev; // path and type are ok
        }
        elseif ($action['action'] == VERSIONCONTROL_SVN_ACTION_REPLACE) {
          // This represents a very slim case, we've got the rare special case that
          // the parent directory has been copied to a new location where the
          // old copied item was swapped against a new one. (Yeah, happens.)
          // It is effectively an in-place replace without any parent
          // information; since there is no no parent/source, we record it as a
          // simple add.
          $action['action'] = VERSIONCONTROL_SVN_ACTION_REPLACE_INPLACE;
        }
        else {
          // Throw a warning here so that everything doesn't break, but it's
          // really clear we've found an edge case.
          throw new Exception("Hit a very, very weird edge case on some operation with a delete component. Revision $this->currentRev, path $path.", E_WARNING);
        }
      } // End machinations for actions with any kind of 'delete' component
    }
    // All done! Send that sucker out for processing.
    return $this->revision;
  }

  static public function itemFromInfo($info) {
    return array(
      'type' => $info['type'],
      'path' => $info['path'],
      'rev' => $info['created_rev'],
    );
  }

  public function procClose($destruct = FALSE) {
    if ($destruct) {
      if (is_object($this->log)) {
        $this->log->procClose($destruct);
      }
      if (is_object($this->info)) {
        $this->info->procClose($destruct);
      }
    }
  }
}

/**
 * Very much like the parent class, just primes revisions with some additional
 * data that's specific to versioncontrol_svn.
 */
class VersioncontrolSvnLogParser extends SvnLogXMLParser {
  protected $actions = array(
    'A' => VERSIONCONTROL_SVN_ACTION_ADD,
    'M' => VERSIONCONTROL_SVN_ACTION_MODIFY,
    'R' => VERSIONCONTROL_SVN_ACTION_REPLACE,
    'D' => VERSIONCONTROL_SVN_ACTION_DELETE,
  );

  protected function parse($rev) {
    $paths = array();
    $revision = array(
      'rev'     => intval((string) $rev['revision']),
      'author'  => (string) $rev->author,
      'msg'     => rtrim($rev->msg), // no trailing linebreaks
      'time_t'  => strtotime($rev->date),
      'paths'   => &$paths,
    );

    if (is_object($rev->paths)) {
      foreach ($rev->paths->path as $logpath) {
        $path = array(
          'path' => (string) $logpath,
          'action' => $this->actions[(string) $logpath['action']],
        );
        if (!empty($logpath['copyfrom-path'])) {
          $path['copyfrom'] = array(
            'path' => (string) $logpath['copyfrom-path'],
            'rev' => (string) $logpath['copyfrom-rev'],
          );
        }
        elseif ($path['action'] == VERSIONCONTROL_SVN_ACTION_DELETE) {
          // First, do the quick-n-easy check to see if this is a move action.
          $move = $rev->xpath("paths/path[@copyfrom-path = '$path[path]']");
          if (!empty($move)) {
            continue;
          }
          // On delete actions, check if this is one of the annoying kind. Run
          // an xpath query, querying for an init-substring match from all of
          // the paths in this rev that are type 'A' and have copyfrom info.
          $parents = $rev->xpath("paths/path[@action = 'A' and @copyfrom-rev and @copyfrom-path != '$path[path]' and starts-with('$path[path]',.)]");
          if (count($parents) > 1) {
            throw new Exception("Multiple possible copy+delete parents found for $path[path]@$revision[rev]; cannot currently handle this.", E_ERROR);
          }
          else {
            $add_path = array_shift($parents);
            if (empty($add_path)) {
              $path['action'] = VERSIONCONTROL_SVN_ACTION_DELETE_SIMPLE;
            }
            elseif (strpos($path['path'], (string) $add_path) !== FALSE) {
              $path['action'] = VERSIONCONTROL_SVN_ACTION_DELETE_UGLY;
              $path['delfrom'] = array(
                'path' => $add_path['copyfrom-path'] . substr($path['path'], strlen($add_path)),
                'rev' => (string) $add_path['copyfrom-rev'],
              );
            }
            else {
              throw new Exception('Blargh', E_ERROR);
            }
          }
        }
        $paths[(string) $logpath] = $path;
      }
    }
    return $revision;
  }
}