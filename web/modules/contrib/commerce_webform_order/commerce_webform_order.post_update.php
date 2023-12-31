<?php

/**
 * @file
 * Post update functions for Commerce Webform Order.
 */

use Drupal\webform\Entity\Webform;

/**
 * Update Commerce Webform Order handlers settings.
 */
function commerce_webform_order_post_update_1(&$sandbox = NULL) {
  if (!isset($sandbox['progress'])) {
    /** @var \Drupal\Core\Entity\EntityTypeManager $entity_type_manager */
    $entity_type_manager = \Drupal::service('entity_type.manager');
    $storage_handler = $entity_type_manager->getStorage('webform');
    $sandbox['ids'] = $storage_handler
      ->getQuery()
      ->accessCheck(FALSE)
      ->execute();
    $sandbox['max'] = count($sandbox['ids']);
    $sandbox['progress'] = 0;
  }

  $ids = array_slice($sandbox['ids'], $sandbox['progress'], 10);

  foreach (Webform::loadMultiple($ids) as $webform) {
    $has_handlers = FALSE;
    foreach ($webform->getHandlers('commerce_webform_order') as $handler) {
      $has_handlers = TRUE;

      $settings = $handler->getSettings();
      $settings = array_replace_recursive($handler->defaultConfiguration(), $settings);

      if (!empty($settings['order_item']['product_variation_entity'])) {
        $settings['order_item']['purchasable_entity_type'] = 'commerce_product_variation';
        $settings['order_item']['purchasable_entity'] = $settings['order_item']['product_variation_entity'];
        unset($settings['order_item']['product_variation_entity']);
      }

      $handler->setSettings($settings);
    }

    if (!empty($has_handlers)) {
      $webform->save();
    }

    $sandbox['progress']++;
  }

  $sandbox['#finished'] = empty($sandbox['max']) ? 1 : ($sandbox['progress'] / $sandbox['max']);

  \Drupal::logger('commerce_webform_order')
    ->debug(
      'Updated @progress of @max webforms.',
      [
        '@progress' => $sandbox['progress'],
        '@max' => $sandbox['max'],
      ]
    );
}
