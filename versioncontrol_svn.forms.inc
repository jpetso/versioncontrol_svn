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

define('VERSIONCONTROL_SVN_MIN_PASSWORD_LENGTH', 5);

/**
 * Implementation of hook_form_alter(): Add elements to various
 * administrative forms that the Version Control API provides.
 */
function versioncontrol_svn_form_alter($form_id, &$form) {
  if ($form['#id'] == 'versioncontrol-repository-form' && $form['#vcs'] == 'svn') {
    versioncontrol_svn_repository_admin_form_alter($form_id, $form);
  }
  else if ($form['#id'] == 'versioncontrol-account-form' && $form['#vcs'] == 'svn') {
    versioncontrol_svn_account_form_alter($form_id, $form);
  }
}


/**
 * Add SVN specific elements to the add/edit repository form.
 */
function versioncontrol_svn_repository_admin_form_alter($form_id, &$form) {
  $repository = $form['#repository'];

  $form['versioncontrol_svn'] = array(
    '#type' => 'value',
    '#value' => TRUE,
  );
  $form['updated'] = array(
    '#type' => 'value',
    '#value' => isset($repository) ? $repository['svn_specific']['updated'] : 0,
  );
  $form['last_revision'] = array(
    '#type' => 'value',
    '#value' => isset($repository) ? $repository['svn_specific']['last_revision'] : 0,
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
    '#default_value' => isset($repository) ? $repository['svn_specific']['auth_username'] : '',
    '#weight' => 1,
    '#size' => 40,
    '#maxlength' => 128,
  );
  $form['repository_information']['svn_authentication']['auth_password'] = array(
    '#type' => 'password',
    '#title' => t('Password'),
    '#description' => t('If empty, the password will not be changed.'),
    '#default_value' => isset($repository) ? $repository['svn_specific']['auth_password'] : '',
    '#weight' => 2,
    '#size' => 40,
    '#maxlength' => 128,
  );

  $form['repository_information']['update_method'] = array(
    '#type' => 'radios',
    '#title' => t('Update method'),
    '#description' => t('Automatic log retrieval requires cron.'),
    '#default_value' => isset($repository)
                        ? $repository['svn_specific']['update_method']
                        : VERSIONCONTROL_SVN_UPDATE_CRON,
    '#weight' => 12,
    '#options' => array(
      VERSIONCONTROL_SVN_UPDATE_CRON => t('Automatic log retrieval.'),
      //VERSIONCONTROL_SVN_UPDATE_XSVN => t('Use external script to insert data.'),
    ),
  );

  $form['repository_information']['allowed_serving_methods'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Serving methods'),
    '#description' => t('Check the serving methods that are being used for this repository. For new Subversion accounts, one of these methods will be assigned to the account.'),
    '#default_value' => versioncontrol_svn_get_allowed_serving_methods($repository),
    '#options' => versioncontrol_svn_get_serving_method_options(),
    '#required' => TRUE,
    '#weight' => 14,
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
    '#default_value' => isset($repository)
                        ? $repository['svn_specific']['path_trunk']
                        : '/%project/trunk',
    '#weight' => 0,
    '#size' => 40,
    '#maxlength' => 255,
  );
  $form['svn_repository_layout']['path_branches'] = array(
    '#type' => 'textfield',
    '#title' => t('Branches directory'),
    '#description' => t('Specify the path of the branches directory here. Use %branch as a placeholder for the branch name and %project for the directory name that was specified in the trunk directory.'),
    '#default_value' => isset($repository)
                        ? $repository['svn_specific']['path_branches']
                        : '/%project/branches/%branch',
    '#weight' => 1,
    '#size' => 40,
    '#maxlength' => 255,
  );
  $form['svn_repository_layout']['path_tags'] = array(
    '#type' => 'textfield',
    '#title' => t('Tags directory'),
    '#description' => t('Specify the path of the tags directory here. Use %tags as a placeholder for the tag name, %branch for the branch name and %project for the directory name that was specified in the trunk directory.'),
    '#default_value' => isset($repository)
                        ? $repository['svn_specific']['path_tags']
                        : '/%project/tags/%branch/%tag',
    '#weight' => 2,
    '#size' => 40,
    '#maxlength' => 255,
  );
}

function versioncontrol_svn_get_serving_method_options() {
  return array(
    VERSIONCONTROL_SVN_SERVER_APACHE_WEBDAV => t('Apache with mod_dav_svn'),
    VERSIONCONTROL_SVN_SERVER_SVNSERVE => t('Plain svnserve'),
    VERSIONCONTROL_SVN_SERVER_SVNSERVE_SSH => t('svnserve over SSH'),
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

  $allowed_serving_methods = 0;
  foreach (array_filter($form_values['allowed_serving_methods']) as $type) {
    $allowed_serving_methods |= $type;
  }

  $svn_specific = array(
    'update_method' => $form_values['update_method'],
    'updated'       => $form_values['updated'],
    'last_revision' => $form_values['last_revision'],
    'auth_username' => $form_values['auth_username'],
    'path_trunk'    => $form_values['path_trunk'],
    'path_branches' => $form_values['path_branches'],
    'path_tags'     => $form_values['path_tags'],
    'allowed_serving_methods' => $allowed_serving_methods,
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
  $header[] = t('Serving method(s)');
  $header[] = t('Update method');
  $header[] = t('Last updated');

  foreach ($rows_by_repo_id as $repo_id => $row) {
    $allowed_serving_methods = versioncontrol_svn_get_allowed_serving_methods($repositories[$repo_id]);
    $serving_methods = versioncontrol_svn_get_serving_method_options();
    foreach ($serving_methods as $key => $description) {
      if (!in_array($key, $allowed_serving_methods)) {
        unset($serving_methods[$key]);
      }
    }
    $rows_by_repo_id[$repo_id][] = theme('item_list', $serving_methods);

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


/**
 * Add SVN specific elements to the edit/register user account form.
 */
function versioncontrol_svn_account_form_alter($form_id, &$form) {
  $repository = $form['#repository'];
  $uid = $form['uid']['#value'];
  $account_creation = empty($form['original_username']['#value']);

  $form['versioncontrol_svn'] = array(
    '#type' => 'value',
    '#value' => TRUE,
  );

  // Add a password field to the form.
  if ($account_creation) {
    $description = t('Choose a password to access the Subversion repository with.');
  }
  else { // editing the account
    $description = t('To change the current Subversion password, enter the new password in both fields.');
  }
  $form['account']['account_password'] = array(
    '#type' => 'password_confirm',
    '#title' => t('Subversion password'),
    '#description' => $description,
    '#weight' => 10,
  );

  // Find out which (or if) serving method is currently assigned to the user.
  $current_serving_method = 0;
  if (!$account_creation) { // creating the account
    $current_serving_method = db_result(db_query(
      'SELECT serving_method FROM {versioncontrol_svn_accounts}
       WHERE uid = %d', $uid
    ));
  }
  $form['original_serving_method'] = array(
    '#type' => 'value',
    '#value' => $current_serving_method,
  );

  $allowed_serving_methods = versioncontrol_svn_get_allowed_serving_methods($repository);
  if ($current_serving_method > 0 && !in_array($current_serving_method, $allowed_serving_methods)) {
    $serving_method_must_change = TRUE;
  }

  if (count($allowed_serving_methods) > 1 || $serving_method_must_change) {
    $serving_method_options = array(
      VERSIONCONTROL_SVN_SERVER_APACHE_WEBDAV => t('HTTP/WebDAV ("svn co https://...")'),
      VERSIONCONTROL_SVN_SERVER_SVNSERVE => t('SVN protocol ("svn co svn://...")'),
      VERSIONCONTROL_SVN_SERVER_SVNSERVE_SSH => t('SVN protocol over SSH ("svn co svn+ssh://...")'),
    );
    foreach ($serving_method_options as $key => $description) {
      if (!in_array($key, $allowed_serving_methods)) {
        unset($serving_method_options[$key]);
      }
    }
    $form['account']['serving_method'] = array(
      '#type' => 'radios',
      '#title' => t('Repository access method'),
      '#description' => t('Choose how you want to communicate with the Subversion server.'),
      '#default_value' => $serving_method_must_change
                          ? reset($allowed_serving_methods) // first available method
                          : $current_serving_method,
      '#options' => $serving_method_options,
      '#required' => TRUE,
      '#weight' => 12,
    );
  }
  else {
    // allowed_serving_methods only contains one element,
    // so its database representation equals the serving method constant.
    $form['serving_method'] = array(
      '#type' => 'value',
      '#value' => $repository['svn_specific']['allowed_serving_methods'],
    );
  }

  $form['#validate']['versioncontrol_svn_account_form_validate'] = array();
}

/**
 * Additional validation for the edit/register user account form.
 */
function versioncontrol_svn_account_form_validate($form_id, $form_values) {
  if (empty($form_values['original_username'])) {
    $account_creation = TRUE;
  }
  else if ($form_values['serving_method'] != $form_values['original_serving_method']) {
    $serving_method_changed = TRUE;
  }
  $password_required = $account_creation || $serving_method_changed;

  if (!$password_required && empty($form_values['account_password'])) {
    return; // The (existing) user didn't change the password, and didn't need to.
  }
  else if (strlen($form_values['account_password']) < VERSIONCONTROL_SVN_MIN_PASSWORD_LENGTH) {
    if ($serving_method_changed) {
      form_set_error('account_password', t('When changing the repository access method, you need to resubmit your password as the authentication process is different.'));
    }
    else {
      form_set_error('account_password', t('The SVN password you have chosen is too short (it must be at least !min characters long).', array('!min' => VERSIONCONTROL_SVN_MIN_PASSWORD_LENGTH)));
    }
  }
}

/**
 * Implementation of hook_versioncontrol_extract_account_data():
 * Extract SVN specific user account additions (say: the password)
 * from the edit/register user account form's submitted values.
 */
function versioncontrol_svn_versioncontrol_extract_account_data($form_values) {
  if (!isset($form_values['versioncontrol_svn']) || empty($form_values['account_password'])) {
    return array();
  }
  switch ($form_values['serving_method']) {
    case VERSIONCONTROL_SVN_SERVER_APACHE_WEBDAV:
      // Apache htpasswd files can use crypt() as one possible encryption method.
    case VERSIONCONTROL_SVN_SERVER_SVNSERVE_SSH:
      // As /etc/passwd also uses crypt(), we might want to have it this way.
      $password = crypt($form_values['account_password']);
      break;
    case VERSIONCONTROL_SVN_SERVER_SVNSERVE:
      // Merely obfuscate the password, we need to get it back in plaintext
      // for the 'users' file (also known as svnserve passwd file,
      // but not related to /etc/passwd).
      $password = crypt($form_values['account_password']);
      break;
  }
  return array(
    'svn_specific' => array(
      'password' => $password,
      'serving_method' => $form_values['serving_method'],
    ),
  );
}
