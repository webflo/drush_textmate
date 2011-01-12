<?php
// $Id$

/**
 * @file
 *  TextMate Commadns for Drush.
 */

/**
 * Implementation of hook_drush_command().
 *
 * @See drush_parse_command() for a list of recognized keys.
 *
 * @return
 *   An associative array describing each command.
 */
function textmate_drush_command() {
  $items = array();

  $items['tm-generate'] = array(
    'description' => "Generate TextMate Snippets.",
    // 'arguments' => array(
    //   'file' => 'Scan a specific file.',
    // ),
    'options' => array(
      // 'core' => 'Core only.',
      // 'contrib' => 'Contrib only.',
      'write' => 'File directory',
    ),
    'examples' => array(
      'drush tm-generate --write=/tmp/d7',
    )
  );
  
  $items['tm-format'] = array(
    'description' => "Generate TextMate completion.",
    'options' => array(
      'plist' => 'plist completion',
      'txt' => 'txt hook list',
    )
  );

  return $items;
}

/**
 * Implementation of hook_drush_help().
 *
 * This function is called whenever a drush user calls
 * 'drush help <name-of-your-command>'
 *
 * @param
 *   A string with the help section (prepend with 'drush:')
 *
 * @return
 *   A string with the help text for your command.
 */
function textmate_drush_help($section) {
  switch ($section) {
    case 'drush:textmate':
      return dt("Generate TextMate Snippets from current drupal installation.");
  }
}

/**
 * Drush TextMate-generate command callback.
 */
function drush_textmate_tm_generate() {
  $version = drush_drupal_major_version();
  if (is_string(drush_get_option('write')) && $dir = realpath(drush_get_option('write'))) {
    if (is_writable($dir)) {
      $write_files = TRUE;
    }
  }

  foreach(drush_scan_directory('./modules', '/\.api\.php$/', array('.', '..', 'CVS')) as $file) {
    include $file->filename;
  }
  
  // get all hooks.
  $functions = get_defined_functions();
  $functions = array_values(array_filter($functions['user'], '_texmate_drush_get_hooks_filter'));
  
  foreach($functions as $function_name) {
    if (strpos($function_name, '::') === FALSE) {
      if (!function_exists($function_name)) {
        return drush_set_error(dt('Function not found'));
      }
      $reflect = new ReflectionFunction($function_name);
    }
    else {
      list($class, $method) = explode('::', $function_name);
      if (!method_exists($class, $method)) {
        return drush_set_error(dt('Method not found'));
      }
      $reflect = new ReflectionMethod($class, $method);
    }
  
    $function = _drush_textmate_print_function($function_name, $reflect->getFileName(), $reflect->getStartLine(), $reflect->getEndLine());
    if (isset($write_files)) {
      file_put_contents($dir . '/' . $function_name . '.' . $version . '.php', $function);  
    } else {
      drush_print_r($function);
    }
  }
}

/**
 * Drush TextMate-format command callback.
 */
function drush_textmate_tm_format() {
  $version = drush_drupal_major_version();
  if (is_string(drush_get_option('write')) && $dir = realpath(drush_get_option('write'))) {
    if (is_writable($dir)) {
      $write_files = TRUE;
    }
  }

  foreach(drush_scan_directory('./modules', '/\.api\.php$/', array('.', '..', 'CVS')) as $file) {
    include $file->filename;
  }
  
  // get all hooks.
  $functions = get_defined_functions();
  $functions = array_values(array_filter($functions['user'], '_texmate_drush_get_hooks_filter'));

  $function = array();
  
  foreach($functions as $function_name) {
    if (strpos($function_name, '::') === FALSE) {
      if (!function_exists($function_name)) {
        return drush_set_error(dt('Function not found'));
      }
      $reflect = new ReflectionFunction($function_name);
    }
    else {
      list($class, $method) = explode('::', $function_name);
      if (!method_exists($class, $method)) {
        return drush_set_error(dt('Method not found'));
      }
      $reflect = new ReflectionMethod($class, $method);
    }
  
    $function[] = _drush_textmate_print_function_name($function_name, $reflect->getFileName(), $reflect->getStartLine(), $reflect->getEndLine());
  }
  
  sort($function);
  
  $output = array();
  
  if (drush_get_option('plist')) {
  foreach($function as $name) {
    $output[] = '{display = '  . "'" . $name . "'" . '; }'; 
  }
  
  $output = "(\n" . implode(",\n", $output) . "\n)";
  } elseif (drush_get_option('txt')) {
    foreach($function as $name) {
      $output[] = $name;
    }
    $output = implode("\n", $output);
  }

    if (isset($write_files)) {
  
      file_put_contents($dir, $output);  
    } else {
      drush_print_r($output);
  }
}

/**
 * Print the specified function
 */
function _drush_textmate_print_function($function_name, $file, $start_line, $end_line) {
  $line_num = 0;
  $doxygen = NULL;
  $fp = fopen($file, 'r');

  $function = '';
  
  while (!feof($fp) && ($line_num < ($start_line - 1))) {
    $line = fgets($fp);
    ++$line_num;
  
    if (substr($line,0,3) == '/**') {
      $doxygen = $line;
    }
    elseif (isset($doxygen)) {
      $doxygen .= $line;
      if ($line_num + 1 == $start_line) {
        // drush_print(rtrim($doxygen));
      }
      if (strstr($line, '*/') !== FALSE) {
        $doxygen = NULL;
      }
    }
  }
  
  $line_function_start = $line_num;
  $start_escape = $line_num + 1;
  $end_escape = $end_line - 1;
  
  $escape = FALSE;
  while (!feof($fp) && ($line_num < $end_line)) {
    $line = fgets($fp);
    
    $done = FALSE;
    
    
    if ($line_function_start == $line_num) {
      $function_name_head = _drush_textmate_function_name($line);
      $line = _texmate_drush_escape_line($line);
      $line = str_replace('function hook_', 'function <?php print $basename; ?>_', $line);
      $done = TRUE;
    } else {
      if ($line_num == $start_escape && $start_escape == $end_escape) {
        $line = '  $1' . "\n" . '}';
        $done = TRUE;
      } elseif ($line_num == $start_escape && $start_escape + 1 == $end_escape) {
        $line = '  ${1:' . trim(_texmate_drush_escape_line($line)) . '}';
        $done = TRUE;
      } elseif ($line_num == $start_escape && $start_escape + 1 < $end_escape) {
        $line = '  ${1:' . trim(_texmate_drush_escape_line($line));
        $done = TRUE;
      }
    }
    
    if ($done == FALSE) {
      if ($line_num == $end_escape - 1) {
        $line = _texmate_drush_escape_line(rtrim($line)) . '}';
      } elseif ($line_num != $end_line - 1) {
        $line = _texmate_drush_escape_line($line);
      }
    }
    
    $body .= rtrim($line) . "\n";
    $_line++;
    $line_num++;
  }

  $head = '/**' . "\n";
  $head .= ' * Implements ' . $function_name_head . '().' . "\n";
  $head .= ' */' . "\n";
  $function = $head . $body . "\n" . '$2';
  
  return rtrim($function);
}

/**
 * Print the specified function
 */
function _drush_textmate_print_function_name($function_name, $file, $start_line, $end_line) {
  $line_num = 0;
  $doxygen = NULL;
  $fp = fopen($file, 'r');

  $function = '';
  
  while (!feof($fp) && ($line_num < ($start_line - 1))) {
    $line = fgets($fp);
    ++$line_num;
  
    if (substr($line,0,3) == '/**') {
      $doxygen = $line;
    }
    elseif (isset($doxygen)) {
      $doxygen .= $line;
      if ($line_num + 1 == $start_line) {
        // drush_print(rtrim($doxygen));
      }
      if (strstr($line, '*/') !== FALSE) {
        $doxygen = NULL;
      }
    }
  }
  
  $line_function_start = $line_num;
  $start_escape = $line_num + 1;
  $end_escape = $end_line - 1;
  
  // drush_print_r($line_function_start . ' :: ' . $start_escape . ' :: ' . $end_escape);
  $escape = FALSE;
  while (!feof($fp) && ($line_num < $end_line)) {
    $line = fgets($fp);
    
    $done = FALSE;
    
    if ($line_function_start == $line_num) {
      return _drush_textmate_function_name($line);
    }
  }
}

function _texmate_drush_escape_line($line) {
  $line = str_replace('$', '\$', $line);
  $line = str_replace('}', '\}', $line);
  return $line;
}

/**
 * Callback function: Called by drush_textmate_tm_generate
 **/
function _texmate_drush_get_hooks_filter($hook) {
  return substr($hook, 0, 5) == 'hook_';
}

function _drush_textmate_function_name($line) {
  $line = explode(' ', $line);
  $function = explode('(', $line[1]);
  return $function[0];
}
