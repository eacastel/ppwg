<?php

/**
 * @file
 * Generate callbacks for the redirect module.
 */

use Drupal\devel_generate\DevelGenerateBase;
use Drupal\node\NodeInterface;
use Drupal\redirect\Entity\Redirect;
use Drupal\Core\Database\Database;

/**
 * @file
 * Devel generate integration for the redirect module.
 */

function redirect_generate_form() {
  $form['count'] = [
    '#type' => 'textfield',
    '#title' => t('How many URL redirects would you like to generate?'),
    '#default_value' => 50,
    '#size' => 4,
  ];
  $form['delete'] = [
    '#type' => 'checkbox',
    '#title' => t('Delete all URL redirects before generating new URL redirects.'),
    '#default_value' => FALSE,
  ];
  $form['submit'] = [
    '#type' => 'submit',
    '#value' => t('Generate'),
  ];

  return $form;
}

function redirect_generate_form_submit(&$form, &$form_state) {
  // Run the batch.
  $batch = redirect_generate_redirects_batch_info($form_state['values']['count'], $form_state['values']['delete']);
  batch_set($batch);
}

function redirect_generate_redirects_batch_info($count, $delete = FALSE) {
  if ($delete) {
    $operations[] = ['redirect_generate_batch_delete', []];
  }

  $operations[] = ['redirect_generate_batch_generate', [$count]];

  return [
    'operations' => $operations,
    'finished' => 'redirect_generate_batch_finished',
    'file' => \Drupal::service('extension.list.module')->getPath('redirect') . '/redirect.generate.inc',
  ];
}

function redirect_generate_batch_delete(array &$context) {
  if (empty($context['sandbox'])) {
    $context['sandbox'] = [];
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['current_rid'] = 0;
    $context['sandbox']['max'] = Database::getConnection()->query('SELECT COUNT(DISTINCT rid) FROM {redirect}')->fetchField();
  }

  $limit = 20;
  $rids = Database::getConnection()->queryRange("SELECT rid FROM {redirect} WHERE rid > :rid ORDER BY rid", 0, $limit, [':rid' => $context['sandbox']['current_rid']])->fetchCol();
  foreach (redirect_repository()->loadMultiple($rids) as $redirect) {
    $redirect->delete();
  }

  // Update our progress information.
  $context['sandbox']['progress'] += count($rids);
  $context['sandbox']['current_rid'] = end($rids);
  $context['message'] = t('Deleted URL redirect @rid.', ['@rid' => end($rids)]);

  // Inform the batch engine that we are not finished,
  // and provide an estimation of the completion level we reached.
  if ($context['sandbox']['progress'] != $context['sandbox']['max']) {
    $context['finished'] = ($context['sandbox']['progress'] >= $context['sandbox']['max']);
  }
}

function redirect_generate_batch_generate($num, array &$context) {
  if (empty($context['sandbox'])) {
    $context['sandbox'] = [];
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = $num;
    $query = \Drupal::database()->select('node', 'n');
    $query->addField('n', 'nid');
    $query->condition('n.status', NodeInterface::PUBLISHED);
    $query->addTag('node_access');
    $context['sandbox']['nids'] = $query->execute()->fetchAllKeyed(0, 0);
  }

  \Drupal::moduleHandler()->loadInclude('devel_generate', 'inc');

  $limit = 20;
  $types = array_keys(redirect_status_code_options());
  $languages = \Drupal::moduleHandler()->moduleExists('locale') ? array_keys(\Drupal::languageManager()->getLanguages()) : [];

  for ($i = 0; $i < min($limit, $context['sandbox']['max'] - $context['sandbox']['progress']); $i++) {
    $rand = mt_rand(0, 100);

    $redirect = Redirect::create();

    $source = _redirect_generate_url();
    $source_options = [];
    $redirect_options = [];

    if ($context['sandbox']['nids'] && $rand >= 40) {
      $redirect_target = 'node/' . array_rand($context['sandbox']['nids']);
    }
    else {
      $redirect_target = _redirect_generate_url(TRUE);
      if ($rand <= 20) {
        $redirect_options['query'] = _redirect_generate_querystring();
      }
      if ($rand <= 5) {
        $redirect_options['fragment'] = DevelGenerateBase::generateWord(mt_rand(4, 8));
      }
    }

    if ($rand <= 20) {
      $redirect->setStatusCode($types[array_rand($types)]);
    }

    if ($languages && $rand <= 20) {
      $redirect->setLanguage($languages[array_rand($languages)]);
    }

    $query = [];
    if ($rand <= 30) {
      $query = _redirect_generate_querystring();
    }

    $redirect->setSource($source, $query);
    $redirect->setRedirect($redirect_target);

    $redirect->save();

    if (mt_rand(0, 1)) {
    $query = \Drupal::database();
    $query->update('redirect')
      ->fields([
        'count' => mt_rand(1, 500),
        'access' => mt_rand(Drupal::time()->getRequestTime() - 31536000, Drupal::time()->getRequestTime()),
      ])
      ->condition('rid', $redirect->id())
      ->execute();
    }

    $context['results'][] = $redirect->id();
  }

  // Update our progress information.
  $context['sandbox']['progress'] += $limit;
  //$context['message'] = t('Deleted URL redirect @rid.', array('@rid' => end($rids)));

  // Inform the batch engine that we are not finished,
  // and provide an estimation of the completion level we reached.
  if ($context['sandbox']['progress'] != $context['sandbox']['max']) {
    $context['finished'] = ($context['sandbox']['progress'] >= $context['sandbox']['max']);
  }
}

function redirect_generate_batch_finished($success, $results, $operations) {
  if ($success) {
    \Drupal::messenger()->addMessage(\Drupal::translation()->formatPlural(count($results), 'One URL redirect created.', '@count URL redirects created.'));
  }
  else {
    // An error occurred.
    // $operations contains the operations that remained unprocessed.
    $error_operation = reset($operations);
    \Drupal::messenger()->addMessage(t('An error occurred while processing @operation with arguments : @args', ['@operation' => $error_operation[0], '@args' => print_r($error_operation[0], TRUE)]));
  }
}

function _redirect_generate_url($external = FALSE, $max_levels = 2) {
  $url = [];
  if ($external) {
    $tlds = ['com', 'net', 'org'];
    $url[] = 'http://www.example.'. $tlds[array_rand($tlds)];
  }
  $max_levels = mt_rand($external ? 0 : 1, $max_levels);
  for ($i = 1; $i <= $max_levels; $i++) {
    $url[] = DevelGenerateBase::generateWord(mt_rand(6 / $i, 8));
  }
  return implode('/', $url);
}

function _redirect_generate_querystring() {
  $query = [DevelGenerateBase::generateWord(mt_rand(1, 3)) => DevelGenerateBase::generateWord(mt_rand(2, 4))];
  return $query;
}
