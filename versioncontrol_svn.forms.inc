<?php
// $Id$
/**
 * @file
 * Subversion backend for Version Control API - Provides Subversion commit
 * information and account management as a pluggable backend.
 *
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 * Copyright 2007 by Adam Light ("aclight", http://drupal.org/user/86358)
 */


/**
 * Implementation of hook_form_alter(): Add elements to various
 * administrative forms that the Version Control API provides.
 */
function versioncontrol_svn_form_alter(&$form, $form_state, $form_id) {
  if ($form['#id'] == 'versioncontrol-repository-form' && $form['#vcs'] == 'svn') {
    versioncontrol_svn_repository_admin_form_alter($form, $form_state, $form_id);
  }
}


/**
 * Add SVN specific elements to the add/edit repository form.
 */
function versioncontrol_svn_repository_admin_form_alter(&$form, $form_state, $form_id) {
  $repository = $form['#repository'];
  $svn_specific = isset($repository) ? $repository['svn_specific'] : array(
    'updated'       => 0,
    'last_revision' => 0,
    'auth_username' => '',
    'auth_password' => '',
    'update_method' => VERSIONCONTROL_SVN_UPDATE_CRON,
    'path_trunk'    => '/%project/trunk',
    'path_branches' => '/%project/branches/%branch',
    'path_tags'     => '/%project/tags/%branch/%tag',
  );

  $form['versioncontrol_svn'] = array(
    '#type' => 'value',
    '#value' => TRUE,
  );
  $form['updated'] = array(
    '#type' => 'value',
    '#value' => $svn_specific['updated'],
  );
  $form['last_revision'] = array(
    '#type' => 'value',
    '#value' => $svn_specific['last_revision'],
  );

  $form['repository_information']['root']['#description'] = t(
    'The URL of this repository. Example: file:///svnroot/repo'
  );

  $form['repository_information']['svn_authentication'] = array(
    '#type' => 'fieldset',
    '#title' => t('Authentication'),
    '#description' => t('If authentication is required in order to be retrieve commit logs and other information from the repository, you need to supply a username and password that will be passed to the \'svn\' executable as \'--username\' and \'--password\' options.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#weight' => 10,
  );
  $form['repository_information']['svn_authentication']['auth_username'] = array(
    '#type' => 'textfield',
    '#title' => t('Username'),
    '#description' => t('Leave empty in order to fetch logs (and other information) anonymously.'),
    '#default_value' => $svn_specific['auth_username'],
    '#weight' => 1,
    '#size' => 40,
    '#maxlength' => 128,
  );
  $form['repository_information']['svn_authentication']['auth_password'] = array(
    '#type' => 'password',
    '#title' => t('Password'),
    '#description' => t('If empty, the password will not be changed.'),
    '#default_value' => $svn_specific['auth_password'],
    '#weight' => 2,
    '#size' => 40,
    '#maxlength' => 128,
  );

  $form['repository_information']['update_method'] = array(
    '#type' => 'radios',
    '#title' => t('Update method'),
    '#description' => t('Automatic log retrieval requires cron.'),
    '#default_value' => $svn_specific['update_method'],
    '#weight' => 12,
    '#options' => array(
      VERSIONCONTROL_SVN_UPDATE_CRON => t('Automatic log retrieval.'),
      //VERSIONCONTROL_SVN_UPDATE_XSVN => t('Use external script to insert data.'),
    ),
  );

  $form['svn_repository_layout'] = array(
    '#type' => 'fieldset',
    '#title' => t('Repository layout'),
    '#description' => t('In order to recognize branches and tags, the Subversion backend needs to know where the trunk, branches and tags directories are located in this repository.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#weight' => 2,
  );
  $form['svn_repository_layout']['path_trunk'] = array(
    '#type' => 'textfield',
    '#title' => t('Trunk directory'),
    '#description' => t('Specify the path of the trunk directory here. Use %project as a placeholder for a directory name (probably the project name) that also occurs in the branches and tags paths.'),
    '#default_value' => $svn_specific['path_trunk'],
    '#weight' => 0,
    '#size' => 40,
    '#maxlength' => 255,
  );
  $form['svn_repository_layout']['path_branches'] = array(
    '#type' => 'textfield',
    '#title' => t('Branches directory'),
    '#description' => t('Specify the path of the branches directory here. Use %branch as a placeholder for the branch name and %project for the directory name that was specified in the trunk directory.'),
    '#default_value' => $svn_specific['path_branches'],
    '#weight' => 1,
    '#size' => 40,
    '#maxlength' => 255,
  );
  $form['svn_repository_layout']['path_tags'] = array(
    '#type' => 'textfield',
    '#title' => t('Tags directory'),
    '#description' => t('Specify the path of the tags directory here. Use %tags as a placeholder for the tag name, %branch for the branch name and %project for the directory name that was specified in the trunk directory.'),
    '#default_value' => $svn_specific['path_tags'],
    '#weight' => 2,
    '#size' => 40,
    '#maxlength' => 255,
  );
}

/**
 * Implementation of hook_versioncontrol_extract_repository_data():
 * Extract SVN specific repository additions from the repository
 * editing/adding form's submitted values.
 */
function versioncontrol_svn_versioncontrol_extract_repository_data($form_values) {
  if (!isset($form_values['versioncontrol_svn'])) {
    return array();
  }

  $svn_specific = array(
    'update_method' => $form_values['update_method'],
    'updated'       => $form_values['updated'],
    'last_revision' => $form_values['last_revision'],
    'auth_username' => $form_values['auth_username'],
    'path_trunk'    => $form_values['path_trunk'],
    'path_branches' => $form_values['path_branches'],
    'path_tags'     => $form_values['path_tags'],
  );
  if (empty($form_values['auth_username'])) {
    $svn_specific['auth_password'] = '';
  }
  else if (!empty($form_values['auth_password'])) {
    $svn_specific['auth_password'] = str_rot13($form_values['auth_password']);
  }
  return array('svn_specific' => $svn_specific);
}

/**
 * Implementation of hook_versioncontrol_alter_repository_list():
 * Add SVN specific columns into the list of Subversion repositories.
 * By changing the @p $header and @p $rows_by_repo_id arguments,
 * the repository list can be customized accordingly.
 *
 * @param $vcs
 *   The unique string identifier for the version control system that
 *   the passed repository list covers.
 * @param $repositories
 *   An array of repositories of the given version control system.
 *   Array keys are the repository ids, and array values are the
 *   repository arrays like returned from versioncontrol_get_repository().
 * @param $header
 *   A list of columns that will be passed to theme('table').
 * @param $rows_by_repo_id
 *   An array of existing table rows, with repository ids as array keys.
 *   Each row already includes the generic column values, and for each row
 *   there is a repository with the same repository id given in the
 *   @p $repositories parameter.
 */
function versioncontrol_svn_versioncontrol_alter_repository_list($vcs, $repositories, &$header, &$rows_by_repo_id) {
  if ($vcs != 'svn') {
    return;
  }
  $header[] = t('Update method');
  $header[] = t('Last updated');

  foreach ($rows_by_repo_id as $repo_id => $row) {
    if ($repositories[$repo_id]['svn_specific']['update_method'] == VERSIONCONTROL_SVN_UPDATE_XSVN) {
      $rows_by_repo_id[$repo_id][] = t('external script');
      $rows_by_repo_id[$repo_id][] = t('n/a');
    }
    else if ($repositories[$repo_id]['svn_specific']['update_method'] == VERSIONCONTROL_SVN_UPDATE_CRON) {
      $rows_by_repo_id[$repo_id][] = t('logs (!fetch)', array(
        '!fetch' => l(t('fetch now'), 'admin/project/versioncontrol-repositories/update/svn/'. $repo_id)
      ));
      $rows_by_repo_id[$repo_id][] = $repositories[$repo_id]['svn_specific']['updated']
        ? t('!date (r!revision)', array(
            '!date' => format_date($repositories[$repo_id]['svn_specific']['updated'], 'small'),
            '!revision' => $repositories[$repo_id]['svn_specific']['last_revision'],
          ))
        : t('never');
    }
  }
}
