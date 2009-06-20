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
   * Full path to the temporary SVN repository.
   */
  protected $repo_path;

  /**
   * URL of the temporary SVN repository.
   */
  protected $repo_url;

  /**
   * Full path of the temporary SVN working copy.
   */
  protected $wc_path;

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
   * The configuration extension to apply to the copied xsvn-config.php file.
   *
   * The config files must be customized for each site, so an addition to the
   * copied file is needed.
   */
  protected $config_ext;

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
   * Full path to the pre-commit hook in the Subversion "hooks" directory.
   */
  protected $pre_commit_hook;

  /**
   * Full path to the source xsvn-pre-commit.php hook.
   */
  protected $pre_commit_file_src;

  /**
   * Full path to the source xsvn-config.php file.
   */
  protected $xsvn_config_src;

  /**
   * Full path to the "xsvn" directory in the temporary Subversion repository.
   */
  protected $xsvn_dir;

  /**
   * Destination of the xsvn-pre-commit.php file.
   */
  protected $pre_commit_file_dst;

  /**
   * Destination of the xsvn-config.php file.
   */
  protected $xsvn_config_dst;

  /**
   * The text of the pre-commit hook file.
   *
   * This is for the pre-commit hook to call the php file, which then does the
   * heavy lifting.
   */
  protected $pre_commit_text;

  /**
   * Help set up a test to use Subversion.
   */
  function setUp() {
    parent::setUp('versioncontrol', 'versioncontrol_svn');

    $this->old_wd              = getcwd();
    $this->file_path           = realpath(file_directory_path());
    $this->repo_path           = $this->file_path .'/svnrepo';
    $this->repo_url            = 'file://'. $this->file_path .'/svnrepo';
    $this->wc_path             = $this->file_path .'/svnwc';
    $this->base_path           = dirname($_SERVER['SCRIPT_FILENAME']);

    // TODO: Make this more portable
    $this->server_user         = $_ENV['APACHE_RUN_USER'];

    $this->config_ext          = "\$xsvn['drupal_path'] = '". $this->base_path ."';\n";
    $this->config_ext         .= "\$GLOBALS['simpletest_db_prefix'] = '". $GLOBALS['db_prefix'] ."';";
    $this->hooks['pre-commit'] = $this->repo_path .'/hooks/pre-commit';
    $module_source             = realpath(drupal_get_path('module', 'versioncontrol_svn'));
    $this->xsvn_src['pre-commit'] = $module_source .'/xsvn/xsvn-pre-commit.php';
    $this->xsvn_files['config'] = $module_source .'/xsvn/xsvn-config.php';
    $this->xsvn_files['dir']            = $this->repo_path .'/hooks/xsvn';
    $this->xsvn_files['pre-commit'] = $this->xsvn_files['dir'] .'/xsvn-pre-commit.php';
    $this->xsvn_files['config'] = $this->xsvn_files['dir'] .'/xsvn-config.php';

    $this->hook_text['pre-commit']     = "#!/bin/sh\n".
      "php ". $this->xsvn_files['pre-commit'] . $this->xsvn_files['config'] .
      ' $@';

    // Create subversion repository
    shell_exec("svnadmin create $this->repo_path");

    // Write the pre-commit hook
    $file = fopen($this->hooks['pre-commit'], 'w');
    fwrite($file, $this->hook_text['pre-commit']);
    fclose($file);
    chmod($this->hooks['pre-commit'], 0755);

    mkdir($this->xsvn_files['dir']);
    mkdir($this->wc_path);

    // Copy the xsvn files
    copy($this->xsvn_src['pre-commit'], $this->xsvn_files['pre-commit']);
    copy($this->xsvn_src['config'], $this->xsvn_files['config']);

    // Append simpletest-specific info to the config file.
    $file = fopen($this->xsvn_files['config'], 'a');
    fwrite($file, $this->config_ext);
    fclose($file);
  }

  /**
   * Return to old working directory and clean up directories.
   */
  function tearDown() {
    chdir($this->old_wd);
    parent::tearDown();
  }

  /**
   * Check out a working copy of the repository.
   *
   * This should be verified to work in a test.
   */
  function wcCheckOut() {
    chdir($this->file_path);
    return shell_exec("svn checkout $this->repo_url $this->wc_path");
  }

  /**
   * Adds a sample file to the working copy.
   *
   * Does not perform any checks for consistency.
   */
  function wcAddSampleFile($path='source.txt', $contents='file contents') {
    chdir($this->wc_path);
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
}
