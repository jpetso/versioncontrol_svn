<?php
// $Id$

/**
 * @file
 * Helper methods for hook test cases.
 *
 * Takes care of the setup and teardown of functionality common to all of the
 * repository hooks.
 */

class SvnHelper extends DrupalWebTestCase {

  /**
   * Root of the temporary SimpleTest files directory.
   *
   * All temporary files are written here.
   */
  protected $file_path;

  /**
   * The old working directory, before chdir().
   */
  protected $old_wd;

  /**
   * The full path to the root of the Drupal installation.
   */
  protected $base_path;

  /**
   * The OS-level user running the web site.
   */
  protected $server_user;

  /**
   * Array containing information about the repository and working copy.
   */
  protected $repo = array();

  /**
   * Array of subversion hook names and their absolute paths, of the form:
   *
   *   array(
   *     'name' => 'path',
   *   );
   */
  protected $hooks = array();

  /**
   * Array of destination xsvn files, such as xsvn-config.php and
   * xsvn-pre-commit.php. Keys are the name of the file, and value is the
   * absolute path, such as:
   *
   *   array(
   *     'pre-commit' => '/path/to/xsvn-pre-commit.php',
   *   );
   */
  protected $xsvn_files = array();

  /**
   * Array of source xsvn files, such as xsvn-config.php and
   * xsvn-pre-commit.php. Keys are the name of the file, and value is the
   * absolute path, such as:
   *
   *   array(
   *     'pre-commit' => '/path/to/src/xsvn-pre-commit.php',
   *   );
   */
  protected $xsvn_src = array();


  /**
   * An array of the texts of the svn hooks. Each SVN hook is a shell script
   * which calls the xsvn-*.php hook with the appropriate arguments.
   */
  protected $hook_text = array();

  /**
   * Help set up a test to use Subversion.
   */
  function setUp() {
    parent::setUp('versioncontrol', 'versioncontrol_svn');

    $this->old_wd                        = getcwd();
    $this->file_path                     = realpath(file_directory_path());
    $this->repo['path']                  = $this->file_path .'/svnrepo';
    $this->repo['url']                   = 'file://'. $this->file_path .'/svnrepo';
    $this->repo['wc_path']               = $this->file_path .'/svnwc';
    $this->base_path                     = dirname($_SERVER['SCRIPT_FILENAME']);

    // TODO: Make this more portable
    $this->server_user                   = $_ENV['APACHE_RUN_USER'];

    $this->hook_text['config']           = "\$xsvn['drupal_path'] = '". $this->base_path ."';\n";
    $this->hook_text['config']          .= "\$GLOBALS['simpletest_db_prefix'] = '". $GLOBALS['db_prefix'] ."';";
    $this->hooks['pre-commit']           = $this->repo['path'] .'/hooks/pre-commit';
    $module_source                       = realpath(drupal_get_path('module', 'versioncontrol_svn'));
    $this->xsvn_src['pre-commit']        = $module_source .'/xsvn/xsvn-pre-commit.php';
    $this->xsvn_files['config']          = $module_source .'/xsvn/xsvn-config.php';
    $this->xsvn_files['dir']             = $this->repo['path'] .'/hooks/xsvn';
    $this->xsvn_files['pre-commit']      = $this->xsvn_files['dir'] .'/xsvn-pre-commit.php';
    $this->xsvn_files['config']          = $this->xsvn_files['dir'] .'/xsvn-config.php';

    $this->hook_text['pre-commit']       = "#!/bin/sh\n".
      "php ". $this->xsvn_files['pre-commit'] . $this->xsvn_files['config'] .
      ' $@';
  }

  /**
   * Return to old working directory and clean up directories.
   */
  function tearDown() {
    chdir($this->old_wd);
    parent::tearDown();
  }

  /**
   * Create the subversion repository.
   */
  function initRepo() {
    mkdir($this->xsvn_files['dir']);
    mkdir($this->repo['wc_path']);

    shell_exec("svnadmin create $this->repo['path']");

    copy($this->xsvn_src['config'], $this->xsvn_files['config']);

    // Append simpletest-specific info to the config file.
    $file = fopen($this->xsvn_files['config'], 'a');
    fwrite($file, $this->config_ext);
    fclose($file);
    $this->repo['created'] = TRUE;
    return;
  }

  /**
   * Copy the named hook to the repository directory. Throws an exception if the
   * repository is not already initialized.
   *
   * @param $hook
   *   The name of the hook to initialize.
   *
   * @return
   *   Null.
   */
  function initHook($hook) {
    if (!$this->repop['created']) {
      throw new Exception(t('Repository has not yet been created.'));
    }

    // Copy the xsvn files
    copy($this->xsvn_src['pre-commit'], $this->xsvn_files['pre-commit']);

    $file = fopen($this->hooks[$hook], 'w');
    fwrite($file, $this->hook_text[$hook]);
    fclose($file);
    chmod($this->hooks[$hook], 0755);

    return;
  }

  /**
   * Initialize the repository in the Drupal database.
   *
   * @param $name
   *   The title to give the repository.
   *
   * @return
   *   The repository object.
   */
  function initDrupalRepo() {
    $menu_path = 'admin/project/versioncontrol-repositories/add-svn';
    $this->repo['name'] = $this->randomName();

    $edit = array();
    $edit['repo_name'] = $name;
    $edit['root'] = $this->repo['url'];
    $edit['update_method'] = VERSIONCONTROL_SVN_UPDATE_XSVN;
    $this->drupalPost($menu_path, $edit, t('Save repository'));
    $repo = array_shift(versioncontrol_get_repositories(array(
                'names' => array($edit['repo_name'])
              )));
    $this->repo['id'] = $repo['repo_id'];

    return $repo;
  }

  /**
   * Create an SVN account for the user with the given uid.
   */
  function initSvnAccount($uid, $username) {
    $menu_path = 'versioncontrol/register/'. $uid . '/'. $this->repo['id'];
    $edit = array();
    $edit['account_name'] = $username;

    $this->drupalPost($menu_path, $edit, t('Create Subversion account'));
  }

  /**
   * Check out a working copy of the repository.
   *
   * This should be verified to work in a test.
   */
  function wcCheckOut() {
    chdir($this->file_path);
    return shell_exec("svn checkout $this->repo['url'] $this->repo['wc_path']");
  }

  /**
   * Adds a sample file to the working copy.
   *
   * Does not perform any checks for consistency.
   */
  function wcAddSampleFile($path='source.txt', $contents='file contents') {
    chdir($this->repo['wc_path']);
    $file = fopen($path, 'w');
    fwrite($file, $contents);
    fclose($file);

    return shell_exec("svn add $path");
  }

  /**
   * Run 'svn commit' with the message given.
   */
  function wcCommit($msg = 'commit message') {
    return shell_exec("svn commit -m '$msg' 2>&1");
  }

  /**
   * Make sure the given operation appears in the commit log on the current
   * page.
   *
   * @param $operation
   *   The operation for which to check.
   */
  function assertOperationLog($op_id, $operation) {
    $this->drupalGet('commitlog?id='. $op_id);
    $this->assertNoText(
      t('No log messages found.'),
      t('Make sure a log message is found.'),
      t('operation log')
    );
    $desc = array_shift($this->elements->xpath("//div[class='description']/pre"));
    $this->assertEqual(
      $desc,
      $operation['message'],
      t('Make sure page message matches operation message.'),
      t('operation log')
    );
  }
}
