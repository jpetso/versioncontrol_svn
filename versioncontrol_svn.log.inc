<?php
// $Id$
/**
 * @file
 * Subversion backend for Version Control API - Provides Subversion commit
 * information and account management as a pluggable backend.
 *
 * This file provides functionality to parse the output of 'svn log' (with
 * support from 'svn info') and transform it into Version Control API commits.
 *
 * Copyright 2007, 2008 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */
include_once(drupal_get_path('module', 'versioncontrol_svn') .'/svnlib/svnlib-deluxe.inc');

/**
 * Actually update the repository by fetching commits and other stuff
 * directly from the repository, invoking the svn executable.
 *
 * @return
 *   TRUE if the logs were updated, NULL if no new entries were fetched
 *   (without errors, though), or FALSE if fetching and updating the logs
 *   failed because of an error.
 */
function _versioncontrol_svn_log_update_repository(&$repository) {
  static $svnlib_mapping = NULL;
  if (!isset($svnlib_mapping)) {
    $svnlib_mapping = array(
      'actions' => array(
        'A' => VERSIONCONTROL_ACTION_ADDED,
        'M' => VERSIONCONTROL_ACTION_MODIFIED,
        'AD' => VERSIONCONTROL_ACTION_MOVED,
        'C' => VERSIONCONTROL_ACTION_COPIED,
        'D' => VERSIONCONTROL_ACTION_DELETED,
      ),
      'types' => array(
        'file' => VERSIONCONTROL_ITEM_FILE,
        'dir' => VERSIONCONTROL_ITEM_DIRECTORY,
      ),
    );
  }

  if (!empty($repository['svn_specific']['auth_username'])) {
    svnlib_set_authentication_info(
      $repository['svn_specific']['auth_username'],
      str_rot13($repository['svn_specific']['auth_password'])
    );
  }
  $last_revision = $repository['svn_specific']['last_revision'];

  $info_items = svnlib_info($repository['root']);
  if (!$info_items) {
    return t('No repository exists at %url.', array('%url' => $repository['root']));
  }
  // By retrieving the repository root, we can even allow
  // non-root repository URLs to be specified in the admin UI.
  $url_item = reset($info_items); // first array element
  $real_root = $url_item['repository_root'];
  $youngest_repository_revision = $url_item['rev'];

  // Don't try to update if there's nothing to update.
  if ($last_revision == $youngest_repository_revision) {
    // Anyways, we want to remember the time when we tried that.
    $repository['svn_specific']['updated'] = time();
    db_query('UPDATE {versioncontrol_svn_repositories}
              SET updated = %d WHERE repo_id = %d',
              $repository['svn_specific']['updated'], $repository['repo_id']);

    return t('No new log entries to fetch.');
  }

  $revisions = svnlib_log(
    $repository['root'], ($last_revision + 1) .':HEAD'
  );
  $revisions = svnlib_more_log_info($revisions, $real_root);
  if (!isset($revisions)) {
    return t('Error when fetching new log entries.');
  }

  foreach ($revisions as $rev_id => $revision) { // processing the oldest revision first
    $commit = array(
      'repository' => $repository,
      'date' => $revision['time_t'],
      'username' => $revision['author'],
      'message' => $revision['msg'],
      'revision' => (string) $rev_id,
      'svn_specific' => array(),
    );
    if (empty($commit['username'])) { // commit inserted by cvs2svn, for example
      $commit['username'] = '(no author)';
      $commit['uid'] = 0;
    }
    $commit_actions = array();

    foreach ($revision['actions'] as $path => $rev_action) {
      $commit_action = array(
        'action' => $svnlib_mapping['actions'][$rev_action['action']],
        'current item' => array(
          'type' => $svnlib_mapping['types'][$rev_action['current_item']['type']],
          'path' => $rev_action['current_item']['path'],
          'revision' => (string) $rev_action['current_item']['rev'],
          'svn_specific' => array(),
        ),
      );
      if ($commit_action['action'] != VERSIONCONTROL_ACTION_ADDED) {
        $commit_action['source items'] = array(
          array(
            'type' => $svnlib_mapping['types'][$rev_action['source_item']['type']],
            'path' => $rev_action['source_item']['path'],
            'revision' => (string) $rev_action['source_item']['rev'],
            'svn_specific' => array(),
          ),
        );
      }
      // Version Control API 5.x-1.x compatibility stuff, deprecated already
      if ($commit_action['action'] == VERSIONCONTROL_ACTION_DELETED) {
        unset($commit_action['current item']);
      }
      // Ok, let's store the commit action the way we have it now.
      $commit_actions[$commit_action['current item']['path']] = $commit_action;
    }

    // Now that was easy, wasn't it? :P
    $commit = versioncontrol_insert_commit($commit, $commit_actions);
    if (isset($commit) && $rev_id > $last_revision) {
      $last_revision = $rev_id;
    }
  }

  if ($last_revision > $repository['svn_specific']['last_revision']) {
    $repository['svn_specific']['last_revision'] = $last_revision;
    $repository['svn_specific']['updated'] = time();

    // Everything's done, remember the last revision that was captured.
    db_query('UPDATE {versioncontrol_svn_repositories}
              SET last_revision = %d, updated = %d WHERE repo_id = %d',
              $repository['svn_specific']['last_revision'],
              $repository['svn_specific']['updated'], $repository['repo_id']);
  }
  return t('Fetched !count new log entries.', array('!count' => count($revisions)));
}
